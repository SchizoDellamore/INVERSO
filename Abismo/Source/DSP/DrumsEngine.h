#pragma once
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
#include <cstdio>

namespace inverso_abismo::drums {

constexpr int maxHits = 32;
constexpr int maxPending = 64;

struct PadSettings {
 int object = 0;
 float tune = 0.f, decay = 1.f, tone = .5f, rattle = 0.f, rust = .2f, variation = .35f, gain = 1.f, pan = 0.f;
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
  const auto& m = objects()[static_cast<std::size_t>(std::clamp(p.object, 0, 15))];
  model = &m;
  const float v = p.variation;
  const float tuneVar = v * 1.5f * rng.next();             // up to +-1.5 st
  const float decayVar = 1.f + .3f * v * rng.next();
  const float levelVar = std::pow(10.f, (3.f * v * rng.next()) / 20.f);
  const float ratio = detail::semitoneRatio(p.tune + tuneVar);
  const float velScale = 1.f - velSens + velSens * velocity;
  gain = p.gain * levelVar * velScale * velScale * objectGain(p.object);
  const float rad = (std::clamp(p.pan, -1.f, 1.f) + 1.f) * .785398f;
  panL = std::cos(rad) * 1.4142f;
  panR = std::sin(rad) * 1.4142f;
  // Harder strikes and higher TONE: shorter, brighter excitation.
  hardness = std::clamp(.25f + .5f * p.tone + .35f * velocity, .05f, 1.f);
  burstLeft = std::max(1, static_cast<int>(m.burstMs * (1.5f - hardness) * .001f * static_cast<float>(sr)));
  excLowpass = 0.f;
  clicksLeft = m.clicks;
  nextClick = 0.0;
  clickInterval = m.clicks > 1 ? sr / std::max(1.f, m.clickRate) : 0.0;
  modeCount = m.modes;
  float longest = .05f;
  for (int i = 0; i < modeCount; ++i) {
   const auto idx = static_cast<std::size_t>(i);
   // RUST pushes modes away from their clean ratios (fixed per pad); variation jiggles them per hit.
   const float spread = 1.f + p.rust * .08f * rustOffsets[idx] + v * .015f * rng.next();
   const float t60 = m.decays[idx] * p.decay * decayVar;
   modes[idx].reset();
   modes[idx].set(m.modeBase * ratio * m.ratios[idx] * (i ? spread : 1.f), t60, sr);
   modeAmp[idx] = m.amps[idx];
   longest = std::max(longest, t60);
  }
  memLevel = m.membrane;
  memFreq = m.baseFreq * ratio;
  memStart = m.memStart;
  memDrop = static_cast<float>(std::exp(-1.0 / std::max(1.0, m.memDropMs * .001 * sr)));
  memDecay = static_cast<float>(std::exp(-6.9078 / std::max(1.0, m.memDecay * p.decay * decayVar * sr)));
  memEnv = 1.f; memPitch = 1.f; memPhase = 0.f;
  if (memLevel > 0.f) longest = std::max(longest, m.memDecay * p.decay);
  noiseLevel = m.noise;
  noiseEnv = 1.f;
  noiseDecay = static_cast<float>(std::exp(-6.9078 / std::max(1.0, m.noiseDecay * p.decay * sr)));
  noiseCoef = 1.f - std::exp(-twoPi * std::min(m.noiseCutoff * (.6f + .8f * p.tone), 18000.f) / static_cast<float>(sr));
  noiseLp = 0.f; noiseHp = 0.f;
  rattle = p.rattle;
  follower = 0.f; buzzGate = 0.f; buzzHold = 0; rattleHp = 0.f;
  const float trainSeconds = m.clicks > 1 ? static_cast<float>(m.clicks) / std::max(1.f, m.clickRate) * 2.f : 0.f;
  lifetime = static_cast<long long>((longest * 1.1f + trainSeconds + .05f) * static_cast<float>(sr));
  const float cutoff = std::clamp(600.f * std::exp2(p.tone * 5.f), 200.f, .45f * static_cast<float>(sr));
  const float g = std::tan(3.14159265f * cutoff / static_cast<float>(sr));
  toneA1 = 1.f / (1.f + g * (g + 1.2f)); toneA2 = g * toneA1; toneA3 = g * toneA2;
  toneFilter.reset();
 }

 void render(float* outL, float* outR, int n) {
  if (!alive) return;
  const auto& m = *model;
  for (int i = 0; i < n; ++i) {
   float exc = 0.f;
   if (burstLeft > 0) { exc += rng.next() * 2.5f; --burstLeft; }
   if (clicksLeft > 1) {
    if (nextClick <= 0.0) {
     exc += rng.next() * 4.f + (rng.next() > 0.f ? 3.f : -3.f);
     --clicksLeft;
     clickInterval *= m.clickDecel;
     nextClick += clickInterval * (1.0 + m.clickJitter * rng.next());
    }
    nextClick -= 1.0;
   }
   excLowpass += hardness * (exc - excLowpass);
   float body = 0.f;
   for (int k = 0; k < modeCount; ++k)
    body += modes[static_cast<std::size_t>(k)].process(excLowpass) * modeAmp[static_cast<std::size_t>(k)];
   body *= .6f;
   float mem = 0.f;
   if (memLevel > 0.f) {
    memPitch = 1.f + (memStart - 1.f) * memEnvPitch();
    memPhase += memFreq * memPitch / static_cast<float>(sr);
    if (memPhase >= 1.f) memPhase -= 1.f;
    mem = detail::sine(memPhase) * memEnv * memLevel;
    memEnv *= memDecay;
   }
   float nz = 0.f;
   if (noiseLevel > 0.f && noiseEnv > 1e-4f) {
    noiseLp += noiseCoef * (rng.next() - noiseLp);
    nz = noiseLp * noiseEnv * noiseLevel * 1.5f;
    noiseEnv *= noiseDecay;
   }
   const float core = body + mem + nz;
   // Rattle: buzzing loose parts, driven by how loud the object is right now.
   float rat = 0.f;
   if (rattle > 0.f) {
    const float a = std::abs(core);
    follower += (a > follower ? .05f : .0015f) * (a - follower);
    if (--buzzHold <= 0) { buzzGate = rng.next() > -.2f ? 1.f : 0.f; buzzHold = 8 + static_cast<int>((rng.next() + 1.f) * 40.f); }
    const float raw = rng.next();
    rattleHp += .25f * (raw - rattleHp);
    rat = (raw - rattleHp) * follower * buzzGate * rattle * 3.f;
   }
   float x = toneFilter.process(core + rat, toneA1, toneA2, toneA3, 1.2f, 0) * gain;
   outL[i] += x * panL;
   outR[i] += x * panR;
   ++elapsed;
  }
  age += n;
  if (elapsed > lifetime) alive = false;
 }
 void stop() { alive = false; }

private:
 float memEnvPitch() { memDropState *= memDrop; return memDropState; }
 const ObjectModel* model = nullptr;
 detail::Random rng;
 std::array<Resonator, 8> modes{};
 std::array<float, 8> modeAmp{};
 SVF toneFilter;
 double sr = 48000.0, nextClick = 0.0, clickInterval = 0.0;
 long long elapsed = 0, lifetime = 0;
 int burstLeft = 0, clicksLeft = 0, modeCount = 0, buzzHold = 0;
 bool alive = false;
 float gain = 1.f, panL = 1.f, panR = 1.f, hardness = .5f, excLowpass = 0.f;
 float memLevel = 0.f, memFreq = 100.f, memStart = 1.f, memDrop = .99f, memDecay = .999f, memEnv = 0.f, memPitch = 1.f;
 float memPhase = 0.f, memDropState = 1.f;
 float noiseLevel = 0.f, noiseEnv = 0.f, noiseDecay = .99f, noiseCoef = .5f, noiseLp = 0.f, noiseHp = 0.f;
 float rattle = 0.f, follower = 0.f, buzzGate = 0.f, rattleHp = 0.f;
 float toneA1 = 1.f, toneA2 = 0.f, toneA3 = 0.f;
public:
 void resetMembraneDrop() { memDropState = 1.f; }
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
   float surface = rng.next() * .004f; // hiss
   if ((rng.next() + 1.f) * .5f < 12.f / static_cast<float>(sr)) crackle = rng.next() * .25f;
   surface += crackle; crackle *= .6f;
   gl += surface; gr += surface;
   l += gramophone * (gl * 1.3f - l);
   r += gramophone * (gr * 1.3f - r);
  }
 }
private:
 struct Coeffs { float a1 = 1.f, a2 = 0.f, a3 = 0.f, k = 1.f; };
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
  reverb.prepare(sr);
  pendingCount = 0;
  // Fixed per-pad "rust" offsets: each pad's object is bent its own way.
  detail::Random r; r.state = 0xBADC0DEu;
  for (auto& pad : rust) for (auto& o : pad) o = r.next();
  smoothed = target;
  derive();
 }
 void setValues(const Values& v) { target = v; }
 void setTransport(double bpm, double ppq, bool playing) { mutilator.setTransport(bpm, ppq, playing); }
 void setPitchBend(float normalised) { bend = std::clamp(normalised, -1.f, 1.f) * 2.f; }
 void setSustain(bool down) { sustainPedal = down; if (!down) saw.sustainUp(); }

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
  chosen->resetMembraneDrop();
  chosen->start(pad, pads[static_cast<std::size_t>(pad)], rust[static_cast<std::size_t>(pad)], velocity, velSens, sr);
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
   p.gain = std::pow(10.f, s[static_cast<std::size_t>(b + P::Level)] / 20.f);
   p.pan = s[static_cast<std::size_t>(b + P::Pan)] * .01f;
  }
  velSens = s[P::VelSens] * .01f;
  sawSettings = {s[P::SawLevel] * .01f, s[P::SawAttack], s[P::SawRelease], s[P::SawVibrato] * .01f, s[P::SawVibRate],
                 s[P::SawGlide], s[P::SawTone] * .01f, s[P::SawWobble] * .01f};
  static const double divisions[7] = {0.0, 1.0, .5, .25, .125, 1.0 / 3.0, 1.0 / 6.0};
  mutilator.configure(divisions[std::clamp(static_cast<int>(std::lround(s[P::StutterDiv])), 0, 6)], s[P::StutterDepth] * .01f,
                      s[P::Chaos] * .01f, s[P::Crush] * .01f, 0.f);
  reverb.configure(s[P::RoomSize] * .01f, s[P::RoomDecay], s[P::RoomDamp] * .01f, false);
  outGain = std::pow(10.f, s[P::Output] / 20.f);
 }

 double sr = 48000.0;
 Values target = defaults(), smoothed = defaults();
 std::array<PadSettings, numPads> pads{};
 std::array<std::array<float, 8>, numPads> rust{};
 std::array<Hit, maxHits> hits{};
 std::array<Pending, maxPending> pending{};
 int pendingCount = 0, controlCountdown = 0;
 Saw saw;
 Saw::Settings sawSettings;
 Character character;
 Mutilator mutilator;
 Reverb reverb;
 detail::Random rng;
 float bend = 0.f, velSens = .7f, outGain = .7f;
 bool sustainPedal = false;
 std::uint32_t triggered = 0;
};

} // namespace inverso_abismo::drums
