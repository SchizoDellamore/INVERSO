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
 float read(const std::array<float, points>& t) const { return readAt(t, phase); }
 float readAt(const std::array<float, points>& t, float at) const {
  const float pos = (at - std::floor(at)) * points;
  const auto i = static_cast<std::size_t>(pos) % points;
  const float f = pos - std::floor(pos);
  return t[i] + f * (t[(i + 1) % points] - t[i]);
 }
};

struct Controls {
 float fita = .7f, harm = 0.f, fork = 0.f, organ = 0.f;
 int wave1 = 0, wave2 = 2;
 float loopMix = .5f, loop1Level = 1.f, loop2Level = 1.f, loop2Ratio = 1.f, wear = .35f, wow = .3f, sat = .3f, fitaCutoff = 6000.f;
 float era = 0.f, breath = .3f, motor = .25f, sag = .5f, speakMs = 60.f;
 float spark = .3f, clang = .5f, maintain = .7f, beatHz = .4f;
 std::array<float, 4> reg{0.f, 1.f, .5f, 0.f};
 int upper = 0;
 float organCutoff = 4000.f, tremDepth = .3f, tremRate = 6.f, speakerBreak = 0.f, hum = .15f;
 float velSens = .4f, octaveSt = 0.f, fineSt = 0.f, glideMs = 0.f, bendRange = 2.f;
 float age = .6f;
};

// Values every voice reads at control rate, computed once by the engine.
struct Shared {
 float loop1Amp = 1.f, loop2Amp = 1.f, loop1Tone = 1.f, loop2Tone = 1.f, fitaCents = 0.f;
 float pressure = 1.f, pressureWobble = 0.f;
 float tremPitch = 0.f, tremAmp = 1.f;
 float vibCents = 0.f; // global vibrato, every machine
 const TapeLoop* tape1 = nullptr; // the loops' flaws; each key reads its own stretch
 const TapeLoop* tape2 = nullptr;
 float wear = 0.f;
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
   forkPh = beatPh = 0.f;
   orgPh = .5f * (rng.next() + 1.f);
   lp1 = lp2 = reedLp = reedLp2 = breathHp = 0.f;
   for (auto* f : {&static1, &static2}) f->reset();
  }
  sparkEnv = 0.f;
  clickEnv = 1.f;
  // Analog life. Each key has its own tuning error and its own stretch of tape;
  // every note gets a fresh drift and imperfect parts.
  std::uint32_t h = static_cast<std::uint32_t>(midiNote + 1) * 2654435761u;
  h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
  keyCents = static_cast<float>(h & 0xFFFFu) / 32767.5f - 1.f;
  keyCents2 = static_cast<float>((h >> 16) & 0xFFFFu) / 32767.5f - 1.f;
  tapeOff1 = .5f * (rng.next() + 1.f);
  tapeOff2 = .5f * (rng.next() + 1.f);
  driftNow = .5f * rng.next(); driftTarget = rng.next(); driftTicks = 0;
  levelNow = .5f * rng.next(); levelTarget = rng.next();
  for (auto& d : orgDuty) d = .5f + .025f * rng.next();
  for (auto& s : orgRound) s = 0.f;
  bounceLeft = 1 + static_cast<int>(2.99f * .5f * (rng.next() + 1.f));
  bounceWait = 30 + static_cast<int>(150.f * .5f * (rng.next() + 1.f));
  if (!stolen) { reed2Ph = .5f * (rng.next() + 1.f); lp1b = lp2b = 0.f; }
  // the fork is struck: its metal rings, the strike itself clicks
  clangPh.fill(0.f);
  sparkY1 = sparkY2 = buzzY1 = buzzY2 = 0.f;
  strikeEnv = 1.f;
  strikeBp.reset();
  amp.gateOn();
  for (auto& e : env) e.gateOn();
 }
 void release() { released = true; amp.gateOff(); for (auto& e : env) e.gateOff(); }
 bool active() const { return amp.active() && (env[0].active() || env[1].active() || env[2].active() || env[3].active()); }
 // One envelope per machine (tape, reed organ, fork, organ), on top of the KEYS envelope.
 std::array<Envelope, 4> env;

 void control(const Controls& c, const Shared& sh, float bendSt, double sr, const Envelope& shape, const std::array<Envelope, 4>& machineShapes) {
  amp.copySettings(shape);
  for (int m = 0; m < 4; ++m) env[static_cast<std::size_t>(m)].copySettings(machineShapes[static_cast<std::size_t>(m)]);
  const float dt = static_cast<float>(controlInterval / sr);
  if (c.glideMs > 0.f) {
   const float step = std::abs(glideStart) * dt * 1000.f / c.glideMs;
   glideOffset = glideOffset > 0.f ? std::max(0.f, glideOffset - step) : std::min(0.f, glideOffset + step);
  } else glideOffset = 0.f;
  const float pitch = static_cast<float>(note - 69) + c.octaveSt + c.fineSt + bendSt + glideOffset + sh.vibCents * .01f;
  const float f = 440.f * detail::semitoneRatio(pitch);
  const float nyq = .45f * static_cast<float>(sr);
  const auto srf = static_cast<float>(sr);
  // Slow wandering of pitch and level, new course every 0.3-1.5 s.
  ageAmt = c.age;
  if (--driftTicks <= 0) {
   driftTarget = rng.next(); levelTarget = rng.next();
   driftTicks = static_cast<int>((.3f + .6f * (rng.next() + 1.f)) / dt);
  }
  const float follow = std::min(1.f, dt * 1.6f);
  driftNow += follow * (driftTarget - driftNow);
  levelNow += follow * (levelTarget - levelNow);
  level = 1.f + ageAmt * .09f * levelNow;
  // FITA: this key's own stretch of each loop (its flaws, dropouts and bends).
  if (sh.tape1 && sh.tape2) {
   const float p1 = sh.tape1->phase + tapeOff1, p2 = sh.tape2->phase + tapeOff2;
   tapeAmp1 = std::max(0.f, 1.f + sh.wear * .45f * sh.tape1->readAt(sh.tape1->amp, p1));
   tapeAmp2 = std::max(0.f, 1.f + sh.wear * .45f * sh.tape2->readAt(sh.tape2->amp, p2));
   tapeTone1 = std::clamp(1.f + sh.wear * .45f * sh.tape1->readAt(sh.tape1->tone, p1), .15f, 1.5f);
   tapeTone2 = std::clamp(1.f + sh.wear * .45f * sh.tape2->readAt(sh.tape2->tone, p2), .15f, 1.5f);
   tapeBend = sh.wear * 9.f * sh.tape1->readAt(sh.tape1->bend, p1);
  }
  const float tapeCents = sh.fitaCents + tapeBend + ageAmt * (4.f * keyCents + 7.f * driftNow);
  const float tapeF = f * detail::semitoneRatio(tapeCents * .01f);
  inc1 = std::min(tapeF, nyq) / srf;
  inc1b = std::min(tapeF * detail::semitoneRatio(.08f), nyq) / srf;
  inc2 = std::min(tapeF * c.loop2Ratio, nyq) / srf;
  inc2b = std::min(tapeF * c.loop2Ratio * detail::semitoneRatio(.08f), nyq) / srf;
  lpCoef1 = dsp::onePoleCoef(c.fitaCutoff * tapeTone1, sr);
  lpCoef2 = dsp::onePoleCoef(c.fitaCutoff * tapeTone2, sr);
  // HARMONIO: the reed speaks flat and rises into pitch; low bellows pressure flattens it.
  const float speak = std::min(1.f, seconds * 1000.f / std::max(5.f, c.speakMs));
  speakLevel = speak * speak * (3.f - 2.f * speak);
  const float reedCents = -22.f * (1.f - speakLevel) - 14.f * c.sag * (1.f - sh.pressure) + ageAmt * (2.5f * keyCents + 3.f * driftNow);
  reedInc = std::min(f * detail::semitoneRatio(reedCents * .01f), nyq) / srf;
  // A second reed tuned a little sharp, as on most reed organs: the slow beating between them.
  reed2Inc = std::min(f * detail::semitoneRatio((reedCents + 3.f + 3.f * std::abs(keyCents2)) * .01f), nyq) / srf;
  reedDuty = .2f - .08f * c.era;
  reedCoef = dsp::onePoleCoef(f * (2.5f + 7.f * sh.pressure) * (1.f + 1.2f * c.era), sr);
  // DIAPASAO: the strike pitch falls back ("boing"), the electric drive builds the sustain.
  const float boing = 1.f + .012f * c.clang * std::exp(-seconds / .06f);
  forkInc = std::min(f * boing * detail::semitoneRatio(ageAmt * (.4f * keyCents + .5f * driftNow) * .01f), nyq) / srf;
  forkBreath = 1.f + ageAmt * .06f * levelNow; // the maintaining circuit never holds the swing perfectly still
  beatInc = std::min(f + c.beatHz, nyq) / srf;
  forkAmp = c.maintain * (.55f + .45f * (1.f - std::exp(-seconds / .25f))) + (1.f - c.maintain) * std::exp(-seconds / 3.5f);
  // The clang: a struck steel bar's modes (6.27, 17.55, 34.39, 56.85 times the
  // fundamental), each split into a close pair that beats, the higher dying faster.
  {
   static const float ratio[6] = {6.267f, 6.301f, 17.55f, 17.63f, 34.39f, 56.85f};
   static const float decay[6] = {.45f, .38f, .14f, .12f, .06f, .035f};
   static const float gain[6] = {1.f, .7f, .5f, .35f, .28f, .18f};
   for (int m = 0; m < 6; ++m) {
    const auto u = static_cast<std::size_t>(m);
    const float hz = f * ratio[m];
    clangInc[u] = hz < nyq ? hz / srf : 0.f;
    clangAmp[u] = hz < nyq ? c.clang * .32f * gain[m] * std::exp(-seconds / decay[m]) : 0.f;
   }
   strikeLevel = c.clang * .9f;
  }
  // The magnet's contact: each cycle it fires, a crack (a short ring high up) and
  // the coil's buzz (a ring lower down); both follow the fork's own pitch.
  {
   auto ring = [&](float hz, float tau, float& a1, float& a2) {
    const float r = std::exp(-1.f / (tau * srf)), w = 6.2831853f * std::min(hz, nyq) / srf;
    a1 = 2.f * r * std::cos(w); a2 = -r * r;
   };
   ring(4200.f + 900.f * keyCents, .00035f, sparkA1, sparkA2);
   ring(1650.f, .0025f, buzzA1, buzzA2);
   const float g = std::tan(3.14159265f * std::min(5200.f, nyq) / srf);
   strikeK = 1.1f; strikeA1 = 1.f / (1.f + g * (g + strikeK)); strikeA2 = g * strikeA1; strikeA3 = g * strikeA2;
   strikeDecay = std::exp(-1.f / (.004f * srf));
  }
  // ORGAO: one master oscillator at 16'; every register divides from it.
  orgInc = std::min(f * .5f * detail::semitoneRatio((sh.tremPitch + ageAmt * (.8f * keyCents + 1.2f * driftNow)) * .01f), nyq / 10.f) / srf;
  // Transistor stages and the RC output filters round each register's square: older, rounder.
  {
   static const float upperMult[3] = {8.f, 6.f, 10.f};
   const float mult[4] = {1.f, 2.f, 4.f, upperMult[std::clamp(c.upper, 0, 2)]};
   for (int k = 0; k < 4; ++k)
    orgRoundCoef[static_cast<std::size_t>(k)] = dsp::onePoleCoef(f * .5f * mult[k] * (40.f - 30.f * ageAmt), sr);
  }
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
   const float e0 = env[0].next(), e1 = env[1].next(), e2 = env[2].next(), e3 = env[3].next();
   if (c.fita > 0.f) {
    // The tape runs a hair unevenly under the head, cycle to cycle.
    const float jitter = 1.f + ageAmt * .0008f * rng.next();
    float a = wave(c.wave1, ph1, inc1 * jitter, ph1b, inc1b * jitter, static1);
    float b = wave(c.wave2, ph2, inc2 * jitter, ph2b, inc2b * jitter, static2);
    // Recorded through a preamp that leans one way (even harmonics), then the
    // head's gentle two-pole roll-off instead of a hard digital edge.
    a += ageAmt * .2f * (a * a - .33f);
    b += ageAmt * .2f * (b * b - .33f);
    lp1 += lpCoef1 * (a - lp1); lp1b += lpCoef1 * (lp1 - lp1b);
    lp2 += lpCoef2 * (b - lp2); lp2b += lpCoef2 * (lp2 - lp2b);
    const float g = e * e0 * c.fita * .11f * level;
    const float x1 = lp1b * tapeAmp1 * (1.f - c.loopMix) * 2.f * c.loop1Level * g, x2 = lp2b * tapeAmp2 * c.loopMix * 2.f * c.loop2Level * g;
    fitaL[i] += .75f * x1 + .25f * x2;
    fitaR[i] += .25f * x1 + .75f * x2;
   }
   if (c.harm > 0.f) {
    float raw = dsp::pulse(reedPh, reedInc, reedDuty);
    reedPh += reedInc; if (reedPh >= 1.f) reedPh -= 1.f;
    if (ageAmt > 0.f) {
     raw = (raw + ageAmt * .45f * dsp::pulse(reed2Ph, reed2Inc, reedDuty * 1.07f)) / (1.f + ageAmt * .3f);
     reed2Ph += reed2Inc; if (reed2Ph >= 1.f) reed2Ph -= 1.f;
     flutter += .003f * (rng.next() - flutter); // the reed tongue never vibrates quite evenly
     raw *= 1.f + ageAmt * .5f * flutter;
    }
    reedLp += reedCoef * (raw - reedLp);
    reedLp2 += reedCoef * (reedLp - reedLp2);
    const float noise = rng.next();
    const float hp = noise - breathHp;
    breathHp += .15f * (noise - breathHp);
    const float p = sh.pressure * (1.f + sh.pressureWobble);
    harm[i] += e * e1 * level * c.harm * speakLevel * (.12f * reedLp2 * p * std::sqrt(p) + c.breath * .03f * hp * (.5f + .5f * p));
   }
   if (c.fork > 0.f) {
    float x = detail::sine(forkPh) * forkAmp;
    if (c.beatHz > .005f) x = .62f * x + .5f * detail::sine(beatPh) * forkAmp;
    for (std::size_t m = 0; m < clangPh.size(); ++m) {
     if (clangAmp[m] < 1e-5f) continue;
     x += clangAmp[m] * detail::sine(clangPh[m]);
     clangPh[m] += clangInc[m]; if (clangPh[m] >= 1.f) clangPh[m] -= 1.f;
    }
    if (strikeEnv > 1e-4f) {
     x += strikeLevel * strikeEnv * strikeBp.process(rng.next(), strikeA1, strikeA2, strikeA3, strikeK, 1);
     strikeEnv *= strikeDecay;
    }
    forkPh += forkInc;
    float kick = 0.f;
    if (forkPh >= 1.f) { forkPh -= 1.f; kick = c.spark * c.maintain * (.8f + .2f * rng.next()); }
    beatPh += beatInc; if (beatPh >= 1.f) beatPh -= 1.f;
    const float crack = kick + sparkA1 * sparkY1 + sparkA2 * sparkY2;
    sparkY2 = sparkY1; sparkY1 = crack;
    const float buzz = kick + buzzA1 * buzzY1 + buzzA2 * buzzY2;
    buzzY2 = buzzY1; buzzY1 = buzz;
    x += .09f * crack + .035f * buzz;
    fork[i] += e * e2 * c.fork * .16f * forkBreath * x;
   }
   if (c.organ > 0.f) {
    float x = 0.f;
    const float t16 = orgPh;
    const float mult[4] = {1.f, 2.f, 4.f, upperMult[std::clamp(c.upper, 0, 2)]};
    for (int k = 0; k < 4; ++k) {
     const auto u = static_cast<std::size_t>(k);
     if (c.reg[u] <= 0.f) continue;
     const float duty = .5f + ageAmt * (orgDuty[u] - .5f); // no two dividers are quite symmetrical
     const float sq = dsp::pulse(dsp::frac(t16 * mult[k]), orgInc * mult[k], duty) - (1.f - 2.f * duty);
     orgRound[u] += orgRoundCoef[u] * (sq - orgRound[u]);
     x += c.reg[u] * (k == 3 ? .8f : 1.f) * orgRound[u];
    }
    orgPh += orgInc; if (orgPh >= 1.f) orgPh -= 1.f;
    // The key contacts click as they close, and bounce once or twice.
    if (bounceLeft > 0 && --bounceWait <= 0) {
     clickEnv = .35f + .4f * std::abs(rng.next());
     --bounceLeft;
     bounceWait = 40 + static_cast<int>(220.f * std::abs(rng.next()));
    }
    clickLp += .35f * (rng.next() - clickLp);
    x = x * regNorm + clickEnv * clickLp * .9f;
    clickEnv *= .993f;
    organ[i] += e * e3 * level * c.organ * .075f * sh.tremAmp * x;
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
 float forkPh = 0.f, beatPh = 0.f, forkInc = 0.f, beatInc = 0.f, forkAmp = 0.f, sparkEnv = 0.f;
 std::array<float, 6> clangPh{}, clangInc{}, clangAmp{};
 float sparkY1 = 0.f, sparkY2 = 0.f, sparkA1 = 0.f, sparkA2 = 0.f, buzzY1 = 0.f, buzzY2 = 0.f, buzzA1 = 0.f, buzzA2 = 0.f;
 float strikeEnv = 0.f, strikeLevel = 0.f, strikeDecay = .99f, strikeA1 = 1.f, strikeA2 = 0.f, strikeA3 = 0.f, strikeK = 1.f;
 SVF strikeBp;
 float orgPh = 0.f, orgInc = 0.f, clickEnv = 0.f, clickLp = 0.f;
 // Analog life.
 float ageAmt = .6f, keyCents = 0.f, keyCents2 = 0.f, driftNow = 0.f, driftTarget = 0.f, levelNow = 0.f, levelTarget = 0.f, level = 1.f;
 int driftTicks = 0, bounceLeft = 0, bounceWait = 0;
 float tapeOff1 = 0.f, tapeOff2 = 0.f, tapeAmp1 = 1.f, tapeAmp2 = 1.f, tapeTone1 = 1.f, tapeTone2 = 1.f, tapeBend = 0.f;
 float lp1b = 0.f, lp2b = 0.f, reed2Ph = 0.f, reed2Inc = 0.f, flutter = 0.f, forkBreath = 1.f;
 std::array<float, 4> orgDuty{.5f, .5f, .5f, .5f}, orgRound{}, orgRoundCoef{1.f, 1.f, 1.f, 1.f};
};

class Engine {
public:
 void prepare(double sampleRate) {
  sr = (std::isfinite(sampleRate) && sampleRate >= 8000.0) ? sampleRate : 48000.0;
  reverb.prepare(sr);
  for (int i = 0; i < maxVoices; ++i) { auto& v = voices[static_cast<std::size_t>(i)]; v.seed(static_cast<std::uint32_t>(i + 1)); v.amp.reset(); for (auto& e : v.env) e.reset(); }
  loop1.build(0x51A7u); loop2.build(0xE40u);
  loop1.phase = 0.f; loop2.phase = .37f;
  const int maxBlock = controlInterval;
  for (auto* b : {&busFitaL, &busFitaR, &busHarm, &busFork, &busOrgan}) b->assign(static_cast<std::size_t>(maxBlock), 0.f);
  chorusL.prepare(static_cast<int>(sr * .04)); chorusR.prepare(static_cast<int>(sr * .04));
  echoL.prepare(static_cast<int>(sr * 1.4)); echoR.prepare(static_cast<int>(sr * 1.4));
  flangerL.prepare(static_cast<int>(sr * .02)); flangerR.prepare(static_cast<int>(sr * .02));
  erL.prepare(static_cast<int>(sr * .04)); erR.prepare(static_cast<int>(sr * .04));
  diffuser[0].prepare(static_cast<int>(sr * .0031), .62f);
  diffuser[1].prepare(static_cast<int>(sr * .0047), .58f);
  diffuser[2].prepare(static_cast<int>(sr * .0073), .55f);
  sideHpCoef = dsp::onePoleCoef(250.f, sr);
  {
   static const float msL[5] = {7.3f, 11.9f, 17.1f, 23.7f, 31.3f}, msR[5] = {8.9f, 13.3f, 19.7f, 26.1f, 29.3f};
   for (int t = 0; t < 5; ++t) {
    erTapL[static_cast<std::size_t>(t)] = msL[t] * .001f * static_cast<float>(sr);
    erTapR[static_cast<std::size_t>(t)] = msR[t] * .001f * static_cast<float>(sr);
   }
  }
  chop.prepare(sr);
  driveL.prepare(sr); driveR.prepare(sr); driveL2.prepare(sr); driveR2.prepare(sr);
  xfadeStep = static_cast<float>(1.0 / (.02 * sr)); // switching drive or filter type: 20 ms crossfade
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
  chosen->control(controls, shared, bend * controls.bendRange, sr, ampShape, machineShapes);
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
  for (auto& v : voices) { v.amp.reset(); for (auto& e : v.env) e.reset(); }
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
   const float ens = s[P::Ensemble] * .01f * onGain[7], echoMix = s[P::EchoMix] * .01f * onGain[8], fb = s[P::EchoFeedback] * .01f, dust = s[P::Dust] * .01f * onGain[9];
   const float echoSamples = fx.echoSamples;
   const float revMix = s[P::RevMix] * .01f * onGain[9];
   const float dryGain = std::cos(revMix * 1.5707963f), wetGain = std::sin(revMix * 1.5707963f) * 1.3f;
   const float width = s[P::Width] * .01f;
   const float anyActivity = std::max({activity[0], activity[1], activity[2], activity[3]});
   for (int i = 0; i < n; ++i) {
    const auto k = static_cast<std::size_t>(i);
    float l = 0.f, r = 0.f, mono = 0.f;
    // FITA bus: saturation and hiss.
    {
     const float drive = 1.f + controls.sat * 4.f;
     const float hiss = rng.next() * controls.wear * .006f * activity[0];
     // Head bump: the playback head lifts the low end around 90 Hz.
     const float bl = busFitaL[k] + bumpGain * bumpL.process(busFitaL[k], bumpA1, bumpA2, bumpA3, bumpK, 1);
     const float br = busFitaR[k] + bumpGain * bumpR.process(busFitaR[k], bumpA1, bumpA2, bumpA3, bumpK, 1);
     l += detail::softTanh(bl * drive) / std::sqrt(drive) * 1.2f + hiss;
     r += detail::softTanh(br * drive) / std::sqrt(drive) * 1.2f + hiss;
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
     mono += x;
    }
    // DIAPASAO bus.
    mono += busFork[k];
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
     // Broken case speaker: a small paper cone (no low end, a honk around
     // 900 Hz, nothing above ~3.5 kHz) with a torn surround and a rubbing coil.
     // Its faults follow the music, not a noise source: when the cone swings
     // far (low notes, loud chords) the coil bottoms out and the loose flap
     // slaps, a buzz that rings at the cone's breakup on every big swing.
     float br = 0.f;
     if (sb > 0.f) {
      float y = spkHp.process(x, spkHpA1, spkHpA2, spkHpA3, .9f, 2);
      y = spkLp.process(y, spkLpA1, spkLpA2, spkLpA3, .8f, 0);
      y += .9f * spkHonk.process(y, honkA1, honkA2, honkA3, .5f, 1) + .45f * spkBreak.process(y, breakA1, breakA2, breakA3, .35f, 1);
      // how far the cone travels: the low part of the signal
      excursion += excursionCoef * (x * 7.f - excursion);
      const float limit = .55f - .4f * sb;
      const float over = std::max(0.f, excursion - limit), under = std::max(0.f, -excursion - limit * 1.6f);
      // coil hitting the back plate (one side only: it sits off centre): the cone stops short
      y *= 1.f / (1.f + 6.f * sb * over);
      // the flap: each swing past the limit kicks a rattle that rings at 1.3 kHz
      const float slap = std::max(0.f, (over + under) - lastOver);
      lastOver = over + under;
      const float flap = slap * 3.f * sb + flapA1 * flapY1 + flapA2 * flapY2;
      flapY2 = flapY1; flapY1 = flap;
      y += flap * .5f;
      // a cracked voice-coil lead: now and then the sound dips for a moment
      if (sb > .45f) {
       if ((rng.next() + 1.f) * .5f < (sb - .45f) * 1.2f / static_cast<float>(sr)) contactLeft = static_cast<int>((.02f + .06f * (rng.next() + 1.f)) * static_cast<float>(sr));
       const float want = contactLeft > 0 ? .3f : 1.f;
       if (contactLeft > 0) --contactLeft;
       contactGain += .004f * (want - contactGain);
       y *= contactGain;
      }
      br = detail::softTanh(y * 1.6f) * .2f; // at about the cabinet's level
     }
     x = (1.f - sb) * cab + sb * br;
     mono += x;
    }
    // The reed organ, fork and organ are mono machines. In a room they never
    // reach both ears the same: a diffused copy above ~250 Hz opens them up,
    // while the low end stays centred.
    {
     float side = mono - sideHp;
     sideHp += sideHpCoef * (mono - sideHp);
     for (auto& ap : diffuser) side = ap.process(side);
     l += mono + width3 * side;
     r += mono - width3 * side;
    }
    // ---- effects rack
    // Drive: a dry/wet blend that reaches full wet at 20% drive, so it eases in
    // from nothing; a change of type crossfades for 20 ms.
    if (fx.driveWet > 0.f || driveFade > 0.f) {
     float wl = driveL.process(l, fx.driveType, fx.drive), wr = driveR.process(r, fx.driveType, fx.drive);
     if (driveFade > 0.f) {
      wl += driveFade * (driveL2.process(l, fx.prevDriveType, fx.drive) - wl);
      wr += driveFade * (driveR2.process(r, fx.prevDriveType, fx.drive) - wr);
      driveFade = std::max(0.f, driveFade - xfadeStep);
     }
     l += fx.driveWet * (wl - l); r += fx.driveWet * (wr - r);
    }
    if ((fx.filterMode > 0 || filterFade > 0.f) && fx.filterOn > 0.f) {
     float lp, bp, hp, lp2, bp2, hp2;
     filterL.processAll(l, fa1, fa2, fa3, fk, lp, bp, hp);
     filterR.processAll(r, fa1, fa2, fa3, fk, lp2, bp2, hp2);
     auto pick = [](int mode, float dry, float a, float b, float c) { return mode == 1 ? a : mode == 2 ? b : mode == 3 ? c : dry; };
     float fl = pick(fx.filterMode, l, lp, bp, hp), fr = pick(fx.filterMode, r, lp2, bp2, hp2);
     if (filterFade > 0.f) {
      fl += filterFade * (pick(fx.prevFilterMode, l, lp, bp, hp) - fl);
      fr += filterFade * (pick(fx.prevFilterMode, r, lp2, bp2, hp2) - fr);
      filterFade = std::max(0.f, filterFade - xfadeStep);
     }
     fl = detail::softTanh(fl * 1.4f) / 1.4f;
     fr = detail::softTanh(fr * 1.4f) / 1.4f;
     l += fx.filterOn * (fl - l); r += fx.filterOn * (fr - r);
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
    // A room around the machines: a few early reflections, different on each side.
    erL.push(l); erR.push(r);
    if (erMix > 0.f) {
     float el = 0.f, rr2 = 0.f;
     for (int t = 0; t < 5; ++t) {
      const auto u = static_cast<std::size_t>(t);
      el += erGain[u] * erL.read(erTapL[u]);
      rr2 += erGain[u] * erR.read(erTapR[u]);
     }
     erLpL += .45f * (rr2 * .4f + el * .6f - erLpL);
     erLpR += .45f * (el * .4f + rr2 * .6f - erLpR);
     l += erMix * erLpL; r += erMix * erLpR;
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
  shared.fitaCents = controls.wow * (14.f * wowValue + 3.f * detail::sine(flutterPhase));
  shared.tape1 = &loop1; shared.tape2 = &loop2; shared.wear = wear;
  // Filter: envelope (restarted by every note) and LFO move the cutoff.
  if (filterAttack) { filterEnv += .35f * (1.f - filterEnv); if (filterEnv > .98f) filterAttack = false; }
  else filterEnv *= std::exp(-dt / std::max(.005f, smoothed[P::FilterDecay] * .001f / 2.3f));
  filterLfoPhase = dsp::frac(filterLfoPhase + dt * smoothed[P::FilterLfoRate]);
  if (fx.filterMode > 0 || filterFade > 0.f) {
   const float oct = fx.filterEnvAmt * 6.f * filterEnv + fx.filterLfo * 3.f * detail::sine(filterLfoPhase);
   const float hz = std::clamp(smoothed[P::Cutoff] * std::exp2(oct), 20.f, static_cast<float>(sr) * .45f);
   const float g = std::tan(3.14159265f * hz / static_cast<float>(sr));
   fk = 2.f - 1.95f * fx.resonance;
   fa1 = 1.f / (1.f + g * (g + fk)); fa2 = g * fa1; fa3 = g * fa2;
  }
  // Global vibrato on every machine.
  vibPhase = dsp::frac(vibPhase + dt * smoothed[P::VibratoRate]);
  shared.vibCents = smoothed[P::VibratoDepth] * onGain[7] * detail::sine(vibPhase);
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
  for (auto& v : voices) if (v.active()) v.control(controls, shared, bend * controls.bendRange, sr, ampShape, machineShapes);
 }

 void derive() {
  const auto& s = smoothed;
  auto pct = [&](int p) { return s[static_cast<std::size_t>(p)] * .01f; };
  auto choice = [&](int p) { return static_cast<int>(std::lround(s[static_cast<std::size_t>(p)])); };
  Controls& c = controls;
  c.fita = pct(P::FitaLevel); c.harm = pct(P::HarmLevel); c.fork = pct(P::ForkLevel); c.organ = pct(P::OrganLevel);
  c.wave1 = choice(P::Loop1Wave); c.wave2 = choice(P::Loop2Wave);
  c.loopMix = pct(P::LoopMix);
  c.loop1Level = pct(P::Loop1Level); c.loop2Level = pct(P::Loop2Level);
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
  for (int m = 0; m < 4; ++m) {
   const int base = P::FitaAttack + 4 * m;
   machineShapes[static_cast<std::size_t>(m)].set(s[static_cast<std::size_t>(base)], s[static_cast<std::size_t>(base + 1)], pct(base + 2), s[static_cast<std::size_t>(base + 3)], sr);
  }
  // Modules switched off fade out (a few ms) instead of stopping dead.
  for (int m = 0; m < 10; ++m) {
   auto& o = onGain[static_cast<std::size_t>(m)];
   const float want = s[static_cast<std::size_t>(P::FitaOn + m)] >= .5f ? 1.f : 0.f;
   o += .25f * (want - o);
   if (std::abs(want - o) < 1e-3f) o = want;
  }
  c.fita *= onGain[0]; c.harm *= onGain[1]; c.fork *= onGain[2]; c.organ *= onGain[3];
  reverb.configure(pct(P::RevSize), s[P::RevDecay], .1f + .8f * pct(P::RevDamp), false);
  c.age = pct(P::Age);
  erMix = .22f * c.age;
  width3 = .15f + .35f * c.age;
  bumpGain = .45f * c.age;
  setBand(90.f, 1.3f, bumpA1, bumpA2, bumpA3, bumpK);
  {
   const int type = choice(P::DriveType);
   if (type != fx.driveType) {
    // the old type carries on from where it is, fading out; the new one fades in
    driveL2 = driveL; driveR2 = driveR;
    fx.prevDriveType = fx.driveType; fx.driveType = type; driveFade = 1.f;
   }
  }
  fx.drive = pct(P::Drive);
  fx.driveWet = std::min(1.f, fx.drive * 5.f) * onGain[4];
  fx.ringMix = pct(P::RingMix) * onGain[4];
  {
   const int mode = choice(P::FilterMode);
   if (mode != fx.filterMode) { fx.prevFilterMode = fx.filterMode; fx.filterMode = mode; filterFade = 1.f; }
  }
  fx.filterOn = onGain[5];
  fx.resonance = pct(P::Resonance);
  fx.filterEnvAmt = pct(P::FilterEnv); fx.filterLfo = pct(P::FilterLfo);
  fx.ringFreq = s[P::RingFreq];
  fx.tremDepth = pct(P::TremoloDepth) * onGain[7]; fx.tremRate = s[P::TremoloRate]; fx.tremShape = choice(P::TremoloShape);
  fx.phaserMix = pct(P::PhaserMix) * onGain[7]; fx.phaserRate = s[P::PhaserRate]; fx.phaserDepth = pct(P::PhaserDepth); fx.phaserFb = .95f * pct(P::PhaserFeedback);
  fx.flangerMix = pct(P::FlangerMix) * onGain[7]; fx.flangerRate = s[P::FlangerRate]; fx.flangerDepth = pct(P::FlangerDepth); fx.flangerFb = .95f * pct(P::FlangerFeedback);
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
   chop.setFreeTime(choice(P::ChopSync) == 1, s[P::ChopMs]);
   const bool chopOn = onGain[6] > .5f;
   chop.setReverse(chopOn ? pct(P::ChopReverse) : 0.f);
   chop.configure(chopOn ? divisions[std::clamp(choice(P::ChopDiv), 0, 6)] : 0.0, pct(P::ChopDepth), pct(P::ChopChaos),
                  pct(P::Crush) * onGain[4], pct(P::Downsample) * onGain[4]);
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
  {
   float k;
   setBand(230.f, .9f, spkHpA1, spkHpA2, spkHpA3, k);
   setBand(3400.f, .8f, spkLpA1, spkLpA2, spkLpA3, k);
   setBand(900.f, .5f, honkA1, honkA2, honkA3, k);
   setBand(2300.f, .35f, breakA1, breakA2, breakA3, k);
   excursionCoef = dsp::onePoleCoef(140.f, sr);
   const float r = std::exp(-1.f / (.006f * static_cast<float>(sr))), w = 6.2831853f * 1300.f / static_cast<float>(sr);
   flapA1 = 2.f * r * std::cos(w); flapA2 = -r * r;
  }
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
 SVF harmBody, cabBoom;
 float harmA1 = 1.f, harmA2 = 0.f, harmA3 = 0.f, harmK = 1.f, harmBodyGain = .7f, harmHp = 0.f, harmLp = 0.f, harmHpCoef = .01f, harmLpCoef = .3f;
 float boomA1 = 1.f, boomA2 = 0.f, boomA3 = 0.f, boomK = 1.f;
 float orgLp = 0.f, orgLp2 = 0.f, orgLpCoef = .3f, cabHp = 0.f;
 SVF spkHp, spkLp, spkHonk, spkBreak;
 float spkHpA1 = 1.f, spkHpA2 = 0.f, spkHpA3 = 0.f, spkLpA1 = 1.f, spkLpA2 = 0.f, spkLpA3 = 0.f;
 float honkA1 = 1.f, honkA2 = 0.f, honkA3 = 0.f, breakA1 = 1.f, breakA2 = 0.f, breakA3 = 0.f;
 float excursion = 0.f, excursionCoef = .01f, lastOver = 0.f, flapY1 = 0.f, flapY2 = 0.f, flapA1 = 0.f, flapA2 = 0.f, contactGain = 1.f;
 int contactLeft = 0;
 float humPhase = 0.f, mainsPhase = 0.f, chorusPhase = 0.f, wowPhase = 0.f, flutterPhase = 0.f, tremPhase = 0.f, fanPhase = 0.f;
 float wowValue = 0.f, echoLpL = 0.f, echoLpR = 0.f, echoTail = 0.f, crackle = 0.f, crackleLp = 0.f;
 float pressure = 1.f, outGain = .5f, bend = 0.f, modWheel = 0.f, aftertouch = 0.f;
 std::array<float, 4> activity{};
 // Effects rack.
 struct Fx {
  int driveType = 0, prevDriveType = 0, filterMode = 0, prevFilterMode = 0, tremShape = 0;
  float driveWet = 0.f, filterOn = 1.f;
  float drive = 0.f, resonance = 0.f, filterEnvAmt = 0.f, filterLfo = 0.f, ringMix = 0.f, ringFreq = 220.f;
  float tremDepth = 0.f, tremRate = 5.f, phaserMix = 0.f, phaserRate = .4f, phaserDepth = .7f, phaserFb = .4f;
  float flangerMix = 0.f, flangerRate = .25f, flangerDepth = .7f, flangerFb = .5f, chorusRate = .55f, chorusDepth = .5f;
  float echoToneCoef = .35f, echoSamples = 18000.f;
 } fx;
 void clearEffects() {
  driveL.clear(); driveR.clear(); driveL2.clear(); driveR2.clear(); driveFade = filterFade = 0.f; filterL.reset(); filterR.reset(); phaserL.clear(); phaserR.clear();
  flangerL.clear(); flangerR.clear(); chop.clear();
  erL.clear(); erR.clear(); erLpL = erLpR = 0.f; bumpL.reset(); bumpR.reset();
  for (auto& ap : diffuser) ap.clear();
  sideHp = 0.f;
  filterEnv = 0.f; filterAttack = false;
 }
 Drive driveL, driveR, driveL2, driveR2;
 float driveFade = 0.f, filterFade = 0.f, xfadeStep = .001f;
 std::array<float, 10> onGain{1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
 std::array<Envelope, 4> machineShapes{};
 SVF filterL, filterR;
 Phaser phaserL, phaserR;
 DelayLine flangerL, flangerR;
 Mutilator chop;
 float fa1 = 1.f, fa2 = 0.f, fa3 = 0.f, fk = 2.f, filterEnv = 0.f, filterLfoPhase = 0.f, vibPhase = 0.f;
 float ringPhase = 0.f, tremoloPhase = 0.f, phaserPhase = 0.f, flangerPhase = 0.f;
 double tempo = 120.0;
 // Stereo diffusion for the mono machines.
 struct Allpass {
  std::vector<float> buf; int at = 0; float g = .6f;
  void prepare(int n, float gain) { buf.assign(static_cast<std::size_t>(std::max(2, n)), 0.f); at = 0; g = gain; }
  void clear() { std::fill(buf.begin(), buf.end(), 0.f); }
  float process(float x) {
   const float d = buf[static_cast<std::size_t>(at)];
   const float y = -g * x + d;
   buf[static_cast<std::size_t>(at)] = x + g * y;
   at = (at + 1) % static_cast<int>(buf.size());
   return y;
  }
 };
 std::array<Allpass, 3> diffuser;
 float sideHp = 0.f, sideHpCoef = .03f, width3 = 0.f;
 // Room reflections and the tape head bump (amount follows AGE).
 DelayLine erL, erR;
 std::array<float, 5> erTapL{}, erTapR{};
 std::array<float, 5> erGain{.42f, .34f, .27f, .21f, .16f};
 float erMix = 0.f, erLpL = 0.f, erLpR = 0.f, bumpGain = 0.f;
 SVF bumpL, bumpR;
 float bumpA1 = 1.f, bumpA2 = 0.f, bumpA3 = 0.f, bumpK = 1.f;
 bool filterAttack = false;
 int controlCountdown = 0, lastNote = -1;
 bool sustainPedal = false;
};

} // namespace inverso_abismo::relicario
