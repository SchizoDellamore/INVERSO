#pragma once
// RELICARIO engine. Portable C++17, no JUCE, no allocation after prepare().
// Four machines share each voice and each has its own bus:
//  FITA     two tape loops of waveforms. The loops have different lengths, so they
//           never line up (Eno); each carries its own flaws (dropouts, dull spots,
//           pitch bends) that come round every pass, plus wow, flutter, hiss, saturation.
//  HARMONIO a free reed on an electric fan bellows: the reed brightens with pressure,
//           takes time to speak, breathes; the fan cannot keep up with chords, so
//           pressure sags, the chord wilts and goes flat.
//  DIAPASAO an electrically maintained tuning fork: a near-pure sine kept alive by
//           an electromagnet whose contact sparks every cycle; the strike adds the
//           fork's 6.27x clang and a falling "boing"; a second fork can beat against it.
//  ORGAO    a Soviet transistor organ: divide-down square waves locked in phase,
//           16'/8'/4' and an upper register (octave, fifth or third), key click,
//           50 Hz mains hum, tremolo on pitch and level, cabinet or broken case speaker.
// Global: tape ensemble, tape echo, dust, reverb, width, output and a soft ceiling.
#include "AbismoCommon.h"
#include "AbismoReverb.h"
#include "RelicarioParams.h"
#include "RelicarioFx.h"
#include "AbismoMutilator.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace inverso_abismo::relicario {

constexpr int maxVoices = 12;

namespace dsp {
inline float onePoleCoef(float hz, double sr) {
 return 1.f - std::exp(-twoPi * std::clamp(hz, 1.f, static_cast<float>(sr) * .45f) / static_cast<float>(sr));
}
inline float saw(float t, float dt) { return 2.f * t - 1.f - detail::polyBlep(t, dt); }
inline float square(float t, float dt) {
 float t2 = t + .5f; if (t2 >= 1.f) t2 -= 1.f;
 return (t < .5f ? 1.f : -1.f) + detail::polyBlep(t, dt) - detail::polyBlep(t2, dt);
}
inline float pulse(float t, float dt, float duty) {
 float t2 = t + 1.f - duty; if (t2 >= 1.f) t2 -= 1.f;
 return (t < duty ? 1.f : -1.f) + detail::polyBlep(t, dt) - detail::polyBlep(t2, dt) + (1.f - 2.f * duty);
}
inline float frac(float x) { return x - std::floor(x); }
}

// One loop of tape: flaws laid along its length, read as it runs past the head.
struct TapeLoop {
 static constexpr int points = 160;
 std::array<float, points> amp{}, tone{}, bend{};
 float phase = 0.f;
 void build(std::uint32_t seed) {
  detail::Random r; r.state = seed;
  for (int i = 0; i < points; ++i) {
   amp[static_cast<std::size_t>(i)] = r.next();
   tone[static_cast<std::size_t>(i)] = r.next();
   bend[static_cast<std::size_t>(i)] = r.next();
  }
  // A few deep dropouts where the oxide has gone.
  for (int k = 0; k < 4; ++k) {
   const auto at = static_cast<std::size_t>((r.next() + 1.f) * .5f * (points - 1));
   amp[at] = -4.f; tone[at] = -2.f;
  }
  for (auto* a : {&amp, &tone, &bend})
   for (int pass = 0; pass < 3; ++pass) { // smooth, wrapping round the splice
    auto copy = *a;
    for (int i = 0; i < points; ++i)
     (*a)[static_cast<std::size_t>(i)] = .25f * copy[static_cast<std::size_t>((i + points - 1) % points)] +
                                         .5f * copy[static_cast<std::size_t>(i)] + .25f * copy[static_cast<std::size_t>((i + 1) % points)];
   }
  // Smoothing shrinks the swings: bring them back to full scale (dropouts reach -1).
  for (auto* a : {&amp, &tone, &bend}) {
   float big = 1e-6f;
   for (float x : *a) big = std::max(big, std::abs(x));
   for (auto& x : *a) x /= big;
  }
 }
 float read(const std::array<float, points>& t) const {
  const float pos = phase * points;
  const auto i = static_cast<std::size_t>(pos) % points;
  const float f = pos - std::floor(pos);
  return t[i] + f * (t[(i + 1) % points] - t[i]);
 }
};

struct Controls {
 float fita = .7f, harm = 0.f, fork = 0.f, organ = 0.f;
 int wave1 = 0, wave2 = 2;
 float loopMix = .5f, loop2Ratio = 1.f, wear = .35f, wow = .3f, sat = .3f, fitaCutoff = 6000.f;
 float era = 0.f, breath = .3f, motor = .25f, sag = .5f, speakMs = 60.f;
 float spark = .3f, clang = .5f, maintain = .7f, beatHz = .4f;
 std::array<float, 4> reg{0.f, 1.f, .5f, 0.f};
 int upper = 0;
 float organCutoff = 4000.f, tremDepth = .3f, tremRate = 6.f, speakerBreak = 0.f, hum = .15f;
 float velSens = .4f, octaveSt = 0.f, fineSt = 0.f, glideMs = 0.f, bendRange = 2.f;
};

// Values every voice reads at control rate, computed once by the engine.
struct Shared {
 float loop1Amp = 1.f, loop2Amp = 1.f, loop1Tone = 1.f, loop2Tone = 1.f, fitaCents = 0.f;
 float pressure = 1.f, pressureWobble = 0.f;
 float tremPitch = 0.f, tremAmp = 1.f;
 float vibCents = 0.f; // global vibrato, every machine
};

class Voice {
public:
 void seed(std::uint32_t s) { rng.state = s * 2654435761u + 999u; }

 void start(int midiNote, float vel, float glideFrom, bool stolen) {
  note = midiNote;
  velocity = std::clamp(vel, 0.f, 1.f);
  released = sustained = false;
  age = 0;
  seconds = 0.f;
  glideOffset = glideStart = glideFrom;
  if (!stolen) {
   ph1 = ph1b = ph2 = ph2b = reedPh = 0.f;
   forkPh = beatPh = clangPh = 0.f;
   orgPh = .5f * (rng.next() + 1.f);
   lp1 = lp2 = reedLp = reedLp2 = breathHp = 0.f;
   for (auto* f : {&static1, &static2}) f->reset();
  }
  sparkEnv = 0.f;
  clickEnv = 1.f;
  amp.gateOn();
 }
 void release() { released = true; amp.gateOff(); }
 bool active() const { return amp.active(); }

 void control(const Controls& c, const Shared& sh, float bendSt, double sr, const Envelope& shape) {
  amp.copySettings(shape);
  const float dt = static_cast<float>(controlInterval / sr);
  if (c.glideMs > 0.f) {
   const float step = std::abs(glideStart) * dt * 1000.f / c.glideMs;
   glideOffset = glideOffset > 0.f ? std::max(0.f, glideOffset - step) : std::min(0.f, glideOffset + step);
  } else glideOffset = 0.f;
  const float pitch = static_cast<float>(note - 69) + c.octaveSt + c.fineSt + bendSt + glideOffset + sh.vibCents * .01f;
  const float f = 440.f * detail::semitoneRatio(pitch);
  const float nyq = .45f * static_cast<float>(sr);
  const auto srf = static_cast<float>(sr);
  // FITA
  const float tapeF = f * detail::semitoneRatio(sh.fitaCents * .01f);
  inc1 = std::min(tapeF, nyq) / srf;
  inc1b = std::min(tapeF * detail::semitoneRatio(.08f), nyq) / srf;
  inc2 = std::min(tapeF * c.loop2Ratio, nyq) / srf;
  inc2b = std::min(tapeF * c.loop2Ratio * detail::semitoneRatio(.08f), nyq) / srf;
  lpCoef1 = dsp::onePoleCoef(c.fitaCutoff * sh.loop1Tone, sr);
  lpCoef2 = dsp::onePoleCoef(c.fitaCutoff * sh.loop2Tone, sr);
  // HARMONIO: the reed speaks flat and rises into pitch; low bellows pressure flattens it.
  const float speak = std::min(1.f, seconds * 1000.f / std::max(5.f, c.speakMs));
  speakLevel = speak * speak * (3.f - 2.f * speak);
  const float reedCents = -22.f * (1.f - speakLevel) - 14.f * c.sag * (1.f - sh.pressure);
  reedInc = std::min(f * detail::semitoneRatio(reedCents * .01f), nyq) / srf;
  reedDuty = .2f - .08f * c.era;
  reedCoef = dsp::onePoleCoef(f * (2.5f + 7.f * sh.pressure) * (1.f + 1.2f * c.era), sr);
  // DIAPASAO: the strike pitch falls back ("boing"), the electric drive builds the sustain.
  const float boing = 1.f + .012f * c.clang * std::exp(-seconds / .06f);
  forkInc = std::min(f * boing, nyq) / srf;
  beatInc = std::min(f + c.beatHz, nyq) / srf;
  clangInc = f * 6.267f < nyq ? f * 6.267f / srf : 0.f;
  forkAmp = c.maintain * (.55f + .45f * (1.f - std::exp(-seconds / .25f))) + (1.f - c.maintain) * std::exp(-seconds / 3.5f);
  clangAmp = c.clang * .5f * std::exp(-seconds / .09f);
  // ORGAO: one master oscillator at 16'; every register divides from it.
  orgInc = std::min(f * .5f * detail::semitoneRatio(sh.tremPitch * .01f), nyq / 10.f) / srf;
  velScale = std::pow(1.f - c.velSens + c.velSens * velocity, 1.3f);
  seconds += dt;
 }

 // Adds this voice to the machine buses.
 void render(const Controls& c, const Shared& sh, float* fitaL, float* fitaR, float* harm, float* fork, float* organ, int n) {
  static const float upperMult[3] = {8.f, 6.f, 10.f};
  const float regSum = c.reg[0] + c.reg[1] + c.reg[2] + c.reg[3];
  const float regNorm = regSum > 0.f ? 1.f / (1.f + .55f * (regSum - 1.f > 0.f ? regSum - 1.f : 0.f)) : 0.f;
  for (int i = 0; i < n; ++i) {
   const float e = amp.next() * velScale;
   if (c.fita > 0.f) {
    const float a = wave(c.wave1, ph1, inc1, ph1b, inc1b, static1);
    const float b = wave(c.wave2, ph2, inc2, ph2b, inc2b, static2);
    lp1 += lpCoef1 * (a - lp1);
    lp2 += lpCoef2 * (b - lp2);
    const float g = e * c.fita * .11f;
    const float x1 = lp1 * sh.loop1Amp * (1.f - c.loopMix) * 2.f * g, x2 = lp2 * sh.loop2Amp * c.loopMix * 2.f * g;
    fitaL[i] += .75f * x1 + .25f * x2;
    fitaR[i] += .25f * x1 + .75f * x2;
   }
   if (c.harm > 0.f) {
    const float raw = dsp::pulse(reedPh, reedInc, reedDuty);
    reedPh += reedInc; if (reedPh >= 1.f) reedPh -= 1.f;
    reedLp += reedCoef * (raw - reedLp);
    reedLp2 += reedCoef * (reedLp - reedLp2);
    const float noise = rng.next();
    const float hp = noise - breathHp;
    breathHp += .15f * (noise - breathHp);
    const float p = sh.pressure * (1.f + sh.pressureWobble);
    harm[i] += e * c.harm * speakLevel * (.12f * reedLp2 * p * std::sqrt(p) + c.breath * .03f * hp * (.5f + .5f * p));
   }
   if (c.fork > 0.f) {
    float x = detail::sine(forkPh) * forkAmp;
    if (c.beatHz > .005f) x = .62f * x + .5f * detail::sine(beatPh) * forkAmp;
    if (clangInc > 0.f) { x += clangAmp * detail::sine(clangPh); clangPh += clangInc; if (clangPh >= 1.f) clangPh -= 1.f; }
    // The contact breaker fires once a cycle: a spark, and a little buzz from the magnet.
    x += .05f * c.spark * c.maintain * detail::sine(dsp::frac(forkPh * 2.f));
    forkPh += forkInc;
    if (forkPh >= 1.f) { forkPh -= 1.f; sparkEnv = c.spark * c.maintain * (.6f + .4f * std::abs(rng.next())); }
    beatPh += beatInc; if (beatPh >= 1.f) beatPh -= 1.f;
    x += sparkEnv * rng.next() * .5f;
    sparkEnv *= .82f;
    fork[i] += e * c.fork * .16f * x;
   }
   if (c.organ > 0.f) {
    float x = 0.f;
    const float t16 = orgPh;
    if (c.reg[0] > 0.f) x += c.reg[0] * dsp::square(t16, orgInc);
    if (c.reg[1] > 0.f) x += c.reg[1] * dsp::square(dsp::frac(t16 * 2.f), orgInc * 2.f);
    if (c.reg[2] > 0.f) x += c.reg[2] * dsp::square(dsp::frac(t16 * 4.f), orgInc * 4.f);
    if (c.reg[3] > 0.f) {
     const float m = upperMult[std::clamp(c.upper, 0, 2)];
     x += c.reg[3] * .8f * dsp::square(dsp::frac(t16 * m), orgInc * m);
    }
    orgPh += orgInc; if (orgPh >= 1.f) orgPh -= 1.f;
    // The key contacts click as they close.
    x = x * regNorm + clickEnv * rng.next() * .5f;
    clickEnv *= .996f;
    organ[i] += e * c.organ * .075f * sh.tremAmp * x;
   }
  }
  age += n;
 }

 int note = -1;
 bool released = false, sustained = false;
 long long age = 0;
 Envelope amp;

private:
 float wave(int type, float& ph, float inc, float& phb, float incb, SVF& st) {
  float x;
  switch (type) {
   case 0: x = dsp::saw(ph, inc); break;
   case 1: x = .65f * (dsp::saw(ph, inc) + dsp::saw(phb, incb)); break;
   case 2: x = 1.f - 4.f * std::abs(ph - .5f); break;
   case 3: x = .8f * dsp::square(ph, inc); break;
   case 4: x = detail::sine(ph); break;
   default: {
    // Static: atmospheric noise through a narrow band at the note.
    const float g = std::tan(3.14159265f * std::min(inc, .45f));
    const float k = .12f;
    const float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
    x = 3.2f * st.process(rng.next(), a1, a2, a3, k, 1);
    break;
   }
  }
  ph += inc; if (ph >= 1.f) ph -= 1.f;
  phb += incb; if (phb >= 1.f) phb -= 1.f;
  return x;
 }

 detail::Random rng;
 float velocity = 1.f, velScale = 1.f, seconds = 0.f, glideOffset = 0.f, glideStart = 0.f;
 float ph1 = 0.f, ph1b = 0.f, ph2 = 0.f, ph2b = 0.f, inc1 = 0.f, inc1b = 0.f, inc2 = 0.f, inc2b = 0.f;
 float lp1 = 0.f, lp2 = 0.f, lpCoef1 = 1.f, lpCoef2 = 1.f;
 SVF static1, static2;
 float reedPh = 0.f, reedInc = 0.f, reedDuty = .2f, reedLp = 0.f, reedLp2 = 0.f, reedCoef = 1.f, breathHp = 0.f, speakLevel = 0.f;
 float forkPh = 0.f, beatPh = 0.f, clangPh = 0.f, forkInc = 0.f, beatInc = 0.f, clangInc = 0.f, forkAmp = 0.f, clangAmp = 0.f, sparkEnv = 0.f;
 float orgPh = 0.f, orgInc = 0.f, clickEnv = 0.f;
};

class Engine {
public:
 void prepare(double sampleRate) {
  sr = (std::isfinite(sampleRate) && sampleRate >= 8000.0) ? sampleRate : 48000.0;
  reverb.prepare(sr);
  for (int i = 0; i < maxVoices; ++i) { voices[static_cast<std::size_t>(i)].seed(static_cast<std::uint32_t>(i + 1)); voices[static_cast<std::size_t>(i)].amp.reset(); }
  loop1.build(0x51A7u); loop2.build(0xE40u);
  loop1.phase = 0.f; loop2.phase = .37f;
  const int maxBlock = controlInterval;
  for (auto* b : {&busFitaL, &busFitaR, &busHarm, &busFork, &busOrgan}) b->assign(static_cast<std::size_t>(maxBlock), 0.f);
  chorusL.prepare(static_cast<int>(sr * .04)); chorusR.prepare(static_cast<int>(sr * .04));
  echoL.prepare(static_cast<int>(sr * 1.4)); echoR.prepare(static_cast<int>(sr * 1.4));
  flangerL.prepare(static_cast<int>(sr * .02)); flangerR.prepare(static_cast<int>(sr * .02));
  chop.prepare(sr);
  clearEffects();
  smoothed = target;
  controlCountdown = 0;
  pressure = 1.f;
  for (auto& a : activity) a = 0.f;
  derive();
 }
 void setValues(const Values& v) { target = v; }

 void noteOn(int note, float velocity) {
  if (velocity <= 0.f) { noteOff(note); return; }
  for (auto& v : voices) if (v.active() && v.note == note && !v.released) v.release();
  Voice* chosen = nullptr;
  bool stolen = false;
  for (auto& v : voices) if (!v.active()) { chosen = &v; break; }
  if (!chosen) {
   stolen = true;
   for (auto& v : voices) if (v.released && (!chosen || v.age > chosen->age)) chosen = &v;
   if (!chosen) for (auto& v : voices) if (!chosen || v.age > chosen->age) chosen = &v;
  }
  const float glideFrom = (controls.glideMs > 0.f && lastNote >= 0) ? static_cast<float>(lastNote - note) : 0.f;
  chosen->start(note, velocity, glideFrom, stolen);
  chosen->control(controls, shared, bend * controls.bendRange, sr, ampShape);
  lastNote = note;
  filterAttack = true; // every note restarts the filter envelope
 }
 void noteOff(int note) {
  for (auto& v : voices) {
   if (!v.active() || v.note != note || v.released) continue;
   if (sustainPedal) v.sustained = true; else v.release();
  }
 }
 void setSustain(bool down) {
  sustainPedal = down;
  if (!down) for (auto& v : voices) if (v.sustained && !v.released) { v.sustained = false; v.release(); }
 }
 void allNotesOff() { sustainPedal = false; for (auto& v : voices) if (v.active()) v.release(); }
 void allSoundOff() {
  for (auto& v : voices) v.amp.reset();
  reverb.clear(); chorusL.clear(); chorusR.clear(); echoL.clear(); echoR.clear();
  for (auto& a : activity) a = 0.f; // hum, hiss and dust stop with the sound
  clearEffects();
 }
 // Host tempo and position, once per block: the chop grid and synced echo follow it.
 void setTransport(double bpm, double ppq, bool playing) {
  chop.setTransport(bpm, ppq, playing);
  if (std::isfinite(bpm) && bpm >= 20.0 && bpm <= 999.0) tempo = bpm;
 }
 void setPitchBend(float normalised) { bend = std::clamp(normalised, -1.f, 1.f); }
 void setModWheel(float v) { modWheel = std::clamp(v, 0.f, 1.f); }
 void setPressure(float v) { aftertouch = std::clamp(v, 0.f, 1.f); }
 int activeVoices() const { int n = 0; for (const auto& v : voices) n += v.active() ? 1 : 0; return n; }
 // For the editor: where each tape loop is, and the bellows pressure.
 float loopPhase(int i) const { return i == 0 ? loop1.phase : loop2.phase; }
 float bellows() const { return pressure; }

 void render(float* left, float* right, int numSamples) {
  int done = 0;
  while (done < numSamples) {
   if (controlCountdown <= 0) { controlTick(); controlCountdown = controlInterval; }
   const int n = std::min(controlCountdown, numSamples - done);
   for (auto* b : {&busFitaL, &busFitaR, &busHarm, &busFork, &busOrgan}) std::fill(b->begin(), b->begin() + n, 0.f);
   for (auto& v : voices) if (v.active()) v.render(controls, shared, busFitaL.data(), busFitaR.data(), busHarm.data(), busFork.data(), busOrgan.data(), n);
   const auto& s = smoothed;
   const float ens = s[P::Ensemble] * .01f, echoMix = s[P::EchoMix] * .01f, fb = s[P::EchoFeedback] * .01f, dust = s[P::Dust] * .01f;
   const float echoSamples = fx.echoSamples;
   const float revMix = s[P::RevMix] * .01f;
   const float dryGain = std::cos(revMix * 1.5707963f), wetGain = std::sin(revMix * 1.5707963f) * 1.3f;
   const float width = s[P::Width] * .01f;
   const float anyActivity = std::max({activity[0], activity[1], activity[2], activity[3]});
   for (int i = 0; i < n; ++i) {
    const auto k = static_cast<std::size_t>(i);
    float l = 0.f, r = 0.f;
    // FITA bus: saturation and hiss.
    {
     const float drive = 1.f + controls.sat * 4.f;
     const float hiss = rng.next() * controls.wear * .006f * activity[0];
     l += detail::softTanh(busFitaL[k] * drive) / std::sqrt(drive) * 1.2f + hiss;
     r += detail::softTanh(busFitaR[k] * drive) / std::sqrt(drive) * 1.2f + hiss;
    }
    // HARMONIO bus: the cabinet (bakelite boxy, plastic nasal) and the fan motor.
    {
     float x = busHarm[k];
     harmHp += harmHpCoef * (x - harmHp); x -= harmHp;
     const float body = harmBody.process(x, harmA1, harmA2, harmA3, harmK, 1);
     x += body * harmBodyGain;
     harmLp += harmLpCoef * (x - harmLp); x = harmLp;
     humPhase += static_cast<float>(59.0 / sr); if (humPhase >= 1.f) humPhase -= 1.f;
     x += controls.motor * activity[1] * .012f * (detail::sine(humPhase) + .5f * detail::sine(dsp::frac(humPhase * 2.f)) + .25f * detail::sine(dsp::frac(humPhase * 3.f)));
     l += x; r += x;
    }
    // DIAPASAO bus.
    l += busFork[k]; r += busFork[k];
    // ORGAO bus: tone filter, mains hum, cabinet or broken case speaker.
    {
     float x = busOrgan[k];
     orgLp += orgLpCoef * (x - orgLp);
     orgLp2 += orgLpCoef * (orgLp - orgLp2);
     x = orgLp2 * 1.15f;
     mainsPhase += static_cast<float>(50.0 / sr); if (mainsPhase >= 1.f) mainsPhase -= 1.f;
     x += controls.hum * activity[3] * .02f * (detail::sine(mainsPhase) + .6f * detail::sine(dsp::frac(mainsPhase * 2.f)) + .4f * detail::sine(dsp::frac(mainsPhase * 3.f)));
     const float sb = controls.speakerBreak;
     // Cabinet: warm, a little boom.
     cabHp += .004f * (x - cabHp);
     const float cab = (x - cabHp) + .35f * cabBoom.process(x, boomA1, boomA2, boomA3, boomK, 1);
     // Broken case: small cone, torn surround buzzing past a small swing.
     float br = x - (brHp += brHpCoef * (x - brHp));
     br = detail::softTanh(br * 2.6f + .25f * br * br * 4.f) * .7f;
     const float rattle = std::max(0.f, std::abs(br) - .12f) * rng.next() * 1.6f;
     br += .6f * brPeak.process(br + rattle, peakA1, peakA2, peakA3, peakK, 1) + rattle;
     // A small paper cone cannot move fast: the fizz stops around 4 kHz.
     brLp += brLpCoef * (br - brLp);
     brLp2 += brLpCoef * (brLp - brLp2);
     br = brLp2 * 1.1f;
     x = (1.f - sb) * cab + sb * br;
     l += x; r += x;
    }
    // ---- effects rack
    if (fx.drive > 0.f) { l = driveL.process(l, fx.driveType, fx.drive); r = driveR.process(r, fx.driveType, fx.drive); }
    if (fx.filterMode > 0) {
     const int mode = fx.filterMode - 1;
     l = detail::softTanh(filterL.process(l, fa1, fa2, fa3, fk, mode) * 1.4f) / 1.4f;
     r = detail::softTanh(filterR.process(r, fa1, fa2, fa3, fk, mode) * 1.4f) / 1.4f;
    }
    if (fx.ringMix > 0.f) {
     ringPhase += fx.ringFreq / static_cast<float>(sr); if (ringPhase >= 1.f) ringPhase -= 1.f;
     const float carrier = detail::sine(ringPhase) * 1.4f;
     l += fx.ringMix * (l * carrier - l); r += fx.ringMix * (r * carrier - r);
    }
    chop.process(l, r);
    if (fx.tremDepth > 0.f) {
     tremoloPhase += fx.tremRate / static_cast<float>(sr); if (tremoloPhase >= 1.f) tremoloPhase -= 1.f;
     float lfo = detail::sine(tremoloPhase);
     if (fx.tremShape == 1) lfo = detail::softTanh(lfo * 6.f); // square, rounded so it does not click
     if (fx.tremShape == 2) { l *= 1.f - fx.tremDepth * (.5f + .5f * lfo); r *= 1.f - fx.tremDepth * (.5f - .5f * lfo); }
     else { const float gain = 1.f - fx.tremDepth * (.5f + .5f * lfo); l *= gain; r *= gain; }
    }
    if (fx.phaserMix > 0.f) {
     phaserPhase += fx.phaserRate / static_cast<float>(sr); if (phaserPhase >= 1.f) phaserPhase -= 1.f;
     const float sweepL = .5f + .5f * detail::sine(phaserPhase), sweepR = .5f + .5f * detail::sine(dsp::frac(phaserPhase + .25f));
     const float cl = Phaser::coefFor(250.f * std::exp2(fx.phaserDepth * 4.5f * sweepL), sr);
     const float cr = Phaser::coefFor(250.f * std::exp2(fx.phaserDepth * 4.5f * sweepR), sr);
     const float pl = phaserL.process(l, cl, fx.phaserFb), pr = phaserR.process(r, cr, fx.phaserFb);
     l += .5f * fx.phaserMix * (pl - l); r += .5f * fx.phaserMix * (pr - r);
    }
    if (fx.flangerMix > 0.f) {
     flangerPhase += fx.flangerRate / static_cast<float>(sr); if (flangerPhase >= 1.f) flangerPhase -= 1.f;
     const float ms = static_cast<float>(sr) * .001f;
     const float dl = ms * (.3f + fx.flangerDepth * 5.f * (.5f + .5f * detail::sine(flangerPhase)));
     const float dr = ms * (.3f + fx.flangerDepth * 5.f * (.5f + .5f * detail::sine(dsp::frac(flangerPhase + .5f))));
     const float wl = flangerL.read(dl), wr = flangerR.read(dr);
     flangerL.push(detail::softTanh(l + wl * fx.flangerFb)); flangerR.push(detail::softTanh(r + wr * fx.flangerFb));
     l += .5f * fx.flangerMix * (wl - l); r += .5f * fx.flangerMix * (wr - r);
    } else { flangerL.push(0.f); flangerR.push(0.f); }
    // Chorus (tape ensemble): two slowly wandering delays.
    chorusL.push(l); chorusR.push(r);
    if (ens > 0.f) {
     chorusPhase += fx.chorusRate / static_cast<float>(sr); if (chorusPhase >= 1.f) chorusPhase -= 1.f;
     const float base = static_cast<float>(sr * .009), depth = static_cast<float>(sr * .007) * fx.chorusDepth;
     const float wl = chorusL.read(base + depth * (.5f + .5f * detail::sine(chorusPhase)));
     const float wr = chorusR.read(base + depth * (.5f + .5f * detail::sine(dsp::frac(chorusPhase + .31f))));
     l = l * (1.f - .45f * ens) + wl * ens * .8f;
     r = r * (1.f - .45f * ens) + wr * ens * .8f;
    }
    // Tape echo: the repeats darken, saturate and wobble.
    if (echoMix > 0.f || echoTail > 1e-5f) {
     const float wob = wowValue * static_cast<float>(sr * .0012);
     const float el = echoL.read(echoSamples + wob), er = echoR.read(echoSamples * 1.03f + wob);
     echoLpL += fx.echoToneCoef * (el - echoLpL); echoLpR += fx.echoToneCoef * (er - echoLpR);
     echoL.push(detail::softTanh(l * echoMix + echoLpR * fb));
     echoR.push(detail::softTanh(r * echoMix + echoLpL * fb));
     l += echoLpL; r += echoLpR;
     echoTail = std::max(std::abs(echoLpL), echoTail * .9999f);
    } else { echoL.push(0.f); echoR.push(0.f); }
    // Dust: crackle on the old machines while they play (and a moment after).
    if (dust > 0.f && anyActivity > .001f) {
     if ((rng.next() + 1.f) * .5f < dust * dust * .004f) crackle = rng.next() * dust * .12f;
     crackleLp += .5f * (crackle - crackleLp);
     const float c = (crackle - crackleLp) * anyActivity;
     crackle *= .6f;
     l += c; r += c * .8f;
    }
    float wl = 0.f, wr = 0.f;
    reverb.process(l, r, wl, wr);
    float ol = l * dryGain + wl * wetGain, orr = r * dryGain + wr * wetGain;
    const float mid = .5f * (ol + orr), side = .5f * (ol - orr) * width;
    left[done + i] = softCeiling((mid + side) * outGain);
    right[done + i] = softCeiling((mid - side) * outGain);
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
  const auto dt = static_cast<float>(controlInterval / sr);
  // Tape loops run past the head; the second is longer, so they drift apart.
  const float len = smoothed[P::LoopLength];
  loop1.phase = dsp::frac(loop1.phase + dt / len);
  loop2.phase = dsp::frac(loop2.phase + dt / (len * 1.37f));
  const float wear = controls.wear;
  shared.loop1Amp = std::max(0.f, 1.f + wear * .45f * loop1.read(loop1.amp));
  shared.loop2Amp = std::max(0.f, 1.f + wear * .45f * loop2.read(loop2.amp));
  shared.loop1Tone = std::clamp(1.f + wear * .45f * loop1.read(loop1.tone), .15f, 1.5f);
  shared.loop2Tone = std::clamp(1.f + wear * .45f * loop2.read(loop2.tone), .15f, 1.5f);
  // Wow (slow), flutter (fast), and the loop's own bends.
  wowPhase = dsp::frac(wowPhase + dt * .63f);
  flutterPhase = dsp::frac(flutterPhase + dt * 7.3f);
  wowValue = detail::sine(wowPhase);
  shared.fitaCents = controls.wow * (14.f * wowValue + 3.f * detail::sine(flutterPhase)) + wear * 9.f * loop1.read(loop1.bend);
  // Filter: envelope (restarted by every note) and LFO move the cutoff.
  if (filterAttack) { filterEnv += .35f * (1.f - filterEnv); if (filterEnv > .98f) filterAttack = false; }
  else filterEnv *= std::exp(-dt / std::max(.005f, smoothed[P::FilterDecay] * .001f / 2.3f));
  filterLfoPhase = dsp::frac(filterLfoPhase + dt * smoothed[P::FilterLfoRate]);
  if (fx.filterMode > 0) {
   const float oct = fx.filterEnvAmt * 6.f * filterEnv + fx.filterLfo * 3.f * detail::sine(filterLfoPhase);
   const float hz = std::clamp(smoothed[P::Cutoff] * std::exp2(oct), 20.f, static_cast<float>(sr) * .45f);
   const float g = std::tan(3.14159265f * hz / static_cast<float>(sr));
   fk = 2.f - 1.95f * fx.resonance;
   fa1 = 1.f / (1.f + g * (g + fk)); fa2 = g * fa1; fa3 = g * fa2;
  }
  // Global vibrato on every machine.
  vibPhase = dsp::frac(vibPhase + dt * smoothed[P::VibratoRate]);
  shared.vibCents = smoothed[P::VibratoDepth] * detail::sine(vibPhase);
  // Tremolo for the organ.
  tremPhase = dsp::frac(tremPhase + dt * controls.tremRate);
  const float trem = detail::sine(tremPhase);
  shared.tremPitch = controls.tremDepth * 16.f * trem;
  shared.tremAmp = 1.f - controls.tremDepth * .32f * (.5f + .5f * trem);
  // The bellows: each held reed note draws air; the fan cannot keep up.
  int held = 0;
  for (const auto& v : voices) held += (v.active() && !v.released) ? 1 : 0;
  const float want = std::clamp(1.f - controls.sag * .075f * static_cast<float>(std::max(0, held - 1)) + aftertouch * .25f, .45f, 1.2f);
  pressure += (want < pressure ? .012f : .004f) * (want - pressure);
  fanPhase = dsp::frac(fanPhase + dt * 6.7f);
  shared.pressure = pressure;
  shared.pressureWobble = controls.motor * .035f * detail::sine(fanPhase);
  // Activity per machine: hum, hiss and dust follow playing and fade after it.
  const float mlev[4] = {controls.fita, controls.harm, controls.fork, controls.organ};
  const bool sounding = activeVoices() > 0;
  for (int m = 0; m < 4; ++m) {
   const float goal = sounding && mlev[m] > 0.f ? 1.f : 0.f;
   activity[static_cast<std::size_t>(m)] += (goal > activity[static_cast<std::size_t>(m)] ? .05f : .0012f) * (goal - activity[static_cast<std::size_t>(m)]);
  }
  for (auto& v : voices) if (v.active()) v.control(controls, shared, bend * controls.bendRange, sr, ampShape);
 }

 void derive() {
  const auto& s = smoothed;
  auto pct = [&](int p) { return s[static_cast<std::size_t>(p)] * .01f; };
  auto choice = [&](int p) { return static_cast<int>(std::lround(s[static_cast<std::size_t>(p)])); };
  Controls& c = controls;
  c.fita = pct(P::FitaLevel); c.harm = pct(P::HarmLevel); c.fork = pct(P::ForkLevel); c.organ = pct(P::OrganLevel);
  c.wave1 = choice(P::Loop1Wave); c.wave2 = choice(P::Loop2Wave);
  c.loopMix = pct(P::LoopMix);
  c.loop2Ratio = std::exp2(static_cast<float>(choice(P::Loop2Octave) - 1)) * detail::semitoneRatio(s[P::Loop2Detune] * .01f);
  c.wear = std::min(1.f, pct(P::Wear) + modWheel * .4f);
  c.wow = std::min(1.f, pct(P::Wow) + modWheel * .5f);
  c.sat = pct(P::TapeSat);
  c.fitaCutoff = 500.f * std::pow(32.f, pct(P::FitaTone));
  c.era = pct(P::Era); c.breath = pct(P::Breath); c.motor = pct(P::Motor); c.sag = pct(P::Sag); c.speakMs = s[P::Speak];
  c.spark = std::min(1.f, pct(P::Spark) + aftertouch * .5f); c.clang = pct(P::Clang); c.maintain = pct(P::Maintain); c.beatHz = s[P::Beat];
  c.reg = {pct(P::Reg16), pct(P::Reg8), pct(P::Reg4), pct(P::Upper)};
  c.upper = choice(P::UpperInterval);
  c.organCutoff = 1100.f * std::pow(12.f, pct(P::OrganTone));
  c.tremDepth = std::min(1.f, pct(P::TremDepth) + modWheel * .4f); c.tremRate = s[P::TremRate];
  c.speakerBreak = pct(P::SpeakerBreak); c.hum = pct(P::Hum);
  c.velSens = pct(P::VelSens);
  c.octaveSt = static_cast<float>((choice(P::Octave) - 2) * 12);
  c.fineSt = s[P::Fine] * .01f;
  c.glideMs = s[P::Glide];
  c.bendRange = s[P::BendRange];
  ampShape.set(s[P::Attack], s[P::Decay], pct(P::Sustain), s[P::Release], sr);
  reverb.configure(pct(P::RevSize), s[P::RevDecay], .1f + .8f * pct(P::RevDamp), false);
  fx.driveType = choice(P::DriveType); fx.drive = pct(P::Drive);
  fx.filterMode = choice(P::FilterMode); fx.resonance = pct(P::Resonance);
  fx.filterEnvAmt = pct(P::FilterEnv); fx.filterLfo = pct(P::FilterLfo);
  fx.ringMix = pct(P::RingMix); fx.ringFreq = s[P::RingFreq];
  fx.tremDepth = pct(P::TremoloDepth); fx.tremRate = s[P::TremoloRate]; fx.tremShape = choice(P::TremoloShape);
  fx.phaserMix = pct(P::PhaserMix); fx.phaserRate = s[P::PhaserRate]; fx.phaserDepth = pct(P::PhaserDepth); fx.phaserFb = .95f * pct(P::PhaserFeedback);
  fx.flangerMix = pct(P::FlangerMix); fx.flangerRate = s[P::FlangerRate]; fx.flangerDepth = pct(P::FlangerDepth); fx.flangerFb = .95f * pct(P::FlangerFeedback);
  fx.chorusRate = s[P::ChorusRate]; fx.chorusDepth = .15f + .85f * pct(P::ChorusDepth);
  fx.echoToneCoef = dsp::onePoleCoef(700.f * std::pow(18.f, pct(P::EchoTone)), sr);
  {
   static const double beats[7] = {0.0, 1.0, .5, .75, .25, 2.0 / 3.0, 1.0 / 3.0};
   const int sync = std::clamp(choice(P::EchoSync), 0, 6);
   const double seconds = sync > 0 ? beats[sync] * 60.0 / tempo : s[P::EchoTime] * .001;
   fx.echoSamples = static_cast<float>(std::min(seconds, 1.35) * sr);
  }
  {
   static const double divisions[7] = {0.0, 1.0, .5, .25, .125, 1.0 / 3.0, 1.0 / 6.0};
   chop.setReverse(pct(P::ChopReverse));
   chop.setFreeTime(choice(P::ChopSync) == 1, s[P::ChopMs]);
   chop.configure(divisions[std::clamp(choice(P::ChopDiv), 0, 6)], pct(P::ChopDepth), pct(P::ChopChaos), pct(P::Crush), pct(P::Downsample));
  }
  outGain = std::pow(10.f, s[P::Output] / 20.f);
  // Reed cabinet: bakelite boxy and dark, plastic thin and nasal.
  const float era = c.era;
  harmHpCoef = dsp::onePoleCoef(60.f + 200.f * era, sr);
  harmLpCoef = dsp::onePoleCoef(3200.f + 4200.f * era, sr);
  setBand(480.f + 1400.f * era, .9f, harmA1, harmA2, harmA3, harmK);
  harmBodyGain = .7f + .5f * era;
  orgLpCoef = dsp::onePoleCoef(c.organCutoff, sr);
  setBand(170.f, 1.2f, boomA1, boomA2, boomA3, boomK);
  brHpCoef = dsp::onePoleCoef(320.f, sr);
  brLpCoef = dsp::onePoleCoef(4200.f, sr);
  setBand(1150.f, .35f, peakA1, peakA2, peakA3, peakK);
 }
 void setBand(float hz, float k, float& a1, float& a2, float& a3, float& kk) const {
  const float g = std::tan(3.14159265f * std::min(hz, static_cast<float>(sr) * .45f) / static_cast<float>(sr));
  kk = k; a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2;
 }

 double sr = 48000.0;
 Values target = defaults(), smoothed = defaults();
 Controls controls;
 Shared shared;
 Envelope ampShape;
 std::array<Voice, maxVoices> voices{};
 TapeLoop loop1, loop2;
 Reverb reverb;
 DelayLine chorusL, chorusR, echoL, echoR;
 std::vector<float> busFitaL, busFitaR, busHarm, busFork, busOrgan;
 detail::Random rng;
 SVF harmBody, cabBoom, brPeak;
 float harmA1 = 1.f, harmA2 = 0.f, harmA3 = 0.f, harmK = 1.f, harmBodyGain = .7f, harmHp = 0.f, harmLp = 0.f, harmHpCoef = .01f, harmLpCoef = .3f;
 float boomA1 = 1.f, boomA2 = 0.f, boomA3 = 0.f, boomK = 1.f, peakA1 = 1.f, peakA2 = 0.f, peakA3 = 0.f, peakK = 1.f;
 float orgLp = 0.f, orgLp2 = 0.f, orgLpCoef = .3f, cabHp = 0.f, brHp = 0.f, brHpCoef = .04f, brLp = 0.f, brLp2 = 0.f, brLpCoef = .4f;
 float humPhase = 0.f, mainsPhase = 0.f, chorusPhase = 0.f, wowPhase = 0.f, flutterPhase = 0.f, tremPhase = 0.f, fanPhase = 0.f;
 float wowValue = 0.f, echoLpL = 0.f, echoLpR = 0.f, echoTail = 0.f, crackle = 0.f, crackleLp = 0.f;
 float pressure = 1.f, outGain = .5f, bend = 0.f, modWheel = 0.f, aftertouch = 0.f;
 std::array<float, 4> activity{};
 // Effects rack.
 struct Fx {
  int driveType = 0, filterMode = 0, tremShape = 0;
  float drive = 0.f, resonance = 0.f, filterEnvAmt = 0.f, filterLfo = 0.f, ringMix = 0.f, ringFreq = 220.f;
  float tremDepth = 0.f, tremRate = 5.f, phaserMix = 0.f, phaserRate = .4f, phaserDepth = .7f, phaserFb = .4f;
  float flangerMix = 0.f, flangerRate = .25f, flangerDepth = .7f, flangerFb = .5f, chorusRate = .55f, chorusDepth = .5f;
  float echoToneCoef = .35f, echoSamples = 18000.f;
 } fx;
 void clearEffects() {
  driveL.clear(); driveR.clear(); filterL.reset(); filterR.reset(); phaserL.clear(); phaserR.clear();
  flangerL.clear(); flangerR.clear(); chop.clear();
  filterEnv = 0.f; filterAttack = false;
 }
 Drive driveL, driveR;
 SVF filterL, filterR;
 Phaser phaserL, phaserR;
 DelayLine flangerL, flangerR;
 Mutilator chop;
 float fa1 = 1.f, fa2 = 0.f, fa3 = 0.f, fk = 2.f, filterEnv = 0.f, filterLfoPhase = 0.f, vibPhase = 0.f;
 float ringPhase = 0.f, tremoloPhase = 0.f, phaserPhase = 0.f, flangerPhase = 0.f;
 double tempo = 120.0;
 bool filterAttack = false;
 int controlCountdown = 0, lastNote = -1;
 bool sustainPedal = false;
};

} // namespace inverso_abismo::relicario
