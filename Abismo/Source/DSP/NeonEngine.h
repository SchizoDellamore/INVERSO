#pragma once
// NEON engine. Each note plays 1..8 unison voices; a voice has two band-limited
// oscillators (PolyBLEP saw / pulse, triangle, sine; hard sync and cross-mod),
// a sub an octave down, noise, and a 2-operator FM pair; then a 24 dB ladder
// (zero-delay feedback, saturating) or a 12 dB state-variable filter; amp,
// filter and FM envelopes. A pattern (arpeggiator or 16-step bass sequencer)
// can play the notes, locked to the host's clock. After the voices: a BBD-style
// chorus (I, II, I+II, Dimension), a ping-pong delay and a reverb (plate, hall
// or gated). Portable (no JUCE); nothing allocates after prepare().
#include "AbismoCommon.h"
#include "AbismoReverb.h"
#include "NeonParams.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace inverso_abismo::neon {

namespace nd {
inline float saw(float t, float dt) { return 2.f * t - 1.f - detail::polyBlep(t, dt); }
inline float pulse(float t, float dt, float duty) {
 float t2 = t + 1.f - duty; if (t2 >= 1.f) t2 -= 1.f;
 return (t < duty ? 1.f : -1.f) + detail::polyBlep(t, dt) - detail::polyBlep(t2, dt);
}
inline float triangle(float t) { return t < .5f ? 4.f * t - 1.f : 3.f - 4.f * t; }
inline float wave(int w, float t, float dt, float duty) {
 switch (w) {
  case 1: return pulse(t, dt, duty);
  case 2: return triangle(t);
  case 3: return detail::sine(t);
  default: return saw(t, dt);
 }
}
inline float midiHz(float note) { return 440.f * std::exp2((note - 69.f) / 12.f); }

// A modulated delay line (linear interpolation), for the chorus.
struct ModDelay {
 std::vector<float> buf;
 int w = 0;
 void prepare(int n) { buf.assign(static_cast<std::size_t>(std::max(8, n)), 0.f); w = 0; }
 void clear() { std::fill(buf.begin(), buf.end(), 0.f); }
 void push(float x) { buf[static_cast<std::size_t>(w)] = x; if (++w >= static_cast<int>(buf.size())) w = 0; }
 float read(float d) const {
  const int n = static_cast<int>(buf.size());
  const float pos = static_cast<float>(w) - std::clamp(d, 1.f, static_cast<float>(n - 3)) + static_cast<float>(n);
  const int i0 = static_cast<int>(pos) % n, i1 = (i0 + 1) % n;
  const float f = pos - std::floor(pos);
  return buf[static_cast<std::size_t>(i0)] + f * (buf[static_cast<std::size_t>(i1)] - buf[static_cast<std::size_t>(i0)]);
 }
};
} // namespace nd

// Everything a voice needs, worked out at control rate from the parameters.
struct Controls {
 int wave1 = 0, wave2 = 0, filterType = 0, fmRatioChoice = 2, voiceMode = 0, unison = 1;
 float oct1 = 0.f, oct2 = 0.f, semi2 = 0.f, fine2 = 0.f, pw = .5f, l1 = .8f, l2 = 0.f, sub = 0.f, noise = 0.f, xmod = 0.f, pwm = 0.f;
 bool sync = false;
 float fmLevel = 0.f, fmRatio = 2.f, fmIndex = .4f, fmDecayCoef = .001f, fmVel = .6f;
 float cutoff = 2400.f, res = .15f, envAmt = .3f, keytrack = .4f, filterVel = .3f, drive = .1f;
 float lfo = 0.f, lfoPitch = 0.f, lfoFilter = 0.f, lfoAmp = 0.f, lfoFadeStep = 1.f;
 float detune = .25f, spread = .6f, glideCoef = 1.f, velSens = .4f, bendSt = 0.f, octaveSt = 0.f, drift = .2f;
};

class Voice {
public:
 int note = -1;
 bool released = false;
 long long age = 0;
 float pan = 0.f, uniGain = 1.f, detuneSt = 0.f;
 Envelope amp, fenv;

 bool active() const { return amp.active(); }
 void seed(std::uint32_t s) { rng.state = s * 2654435761u + 12345u; }
 void start(int n, float velocity, float fromPitch, bool retrigger, float accent) {
  note = n; released = false; age = 0;
  vel = velocity; accentBoost = accent;
  target = static_cast<float>(n);
  pitch = fromPitch < 0.f ? target : fromPitch;
  if (retrigger || !amp.active()) {
   amp.gateOn(); fenv.gateOn();
   fmEnv = 1.f; lfoFade = 0.f;
   if (!wasPlaying()) { ph1 = rng.next() * .5f + .5f; ph2 = rng.next() * .5f + .5f; phSub = ph1 * .5f; }
  }
 }
 void glideTo(int n, float velocity, float accent) { note = n; target = static_cast<float>(n); vel = velocity; accentBoost = accent; released = false; }
 void release() { released = true; amp.gateOff(); fenv.gateOff(); }
 void kill() { amp.reset(); fenv.reset(); note = -1; s1 = s2 = s3 = s4 = 0.f; svf.reset(); }

 void control(const Controls& c, double sr) {
  const auto srf = static_cast<float>(sr);
  pitch += c.glideCoef * (target - pitch);
  // slow analog drift, a new course every so often
  if (--driftTicks <= 0) { driftTarget = rng.next(); driftTicks = 400 + static_cast<int>((rng.next() + 1.f) * 600.f); }
  driftNow += .002f * (driftTarget - driftNow);
  lfoFade = std::min(1.f, lfoFade + c.lfoFadeStep);
  const float vib = c.lfoPitch * c.lfo * lfoFade * .6f; // up to +-0.6 st
  const float base = pitch + c.bendSt + c.octaveSt + detuneSt + driftNow * c.drift * .12f + vib;
  const float nyq = .48f * srf;
  inc1 = std::min(nyq, nd::midiHz(base + 12.f * c.oct1)) / srf;
  inc2 = std::min(nyq, nd::midiHz(base + 12.f * c.oct2 + c.semi2 + c.fine2)) / srf;
  incSub = std::min(nyq, nd::midiHz(base + 12.f * c.oct1 - 12.f)) / srf;
  const float f0 = nd::midiHz(base);
  incCar = std::min(nyq, f0) / srf;
  incMod = std::min(nyq, f0 * c.fmRatio) / srf;
  // filter: cutoff moves in octaves
  const float velAmt = c.filterVel * (vel - .5f) * 2.f;
  const float oct = c.envAmt * fenvNow * 7.f + c.keytrack * (pitch - 60.f) / 12.f + c.lfoFilter * c.lfo * lfoFade * 2.f + velAmt + accentBoost * 1.5f;
  const float fc = std::clamp(c.cutoff * std::exp2(oct), 20.f, .45f * srf);
  g = std::tan(3.14159265f * fc / srf);
  G = g / (1.f + g);
  k = c.res * 4.1f;            // ladder: self-oscillates near the top
  svfK = 2.f - 1.96f * c.res;  // 12 dB: 1/Q
  a1 = 1.f / (1.f + g * (g + svfK)); a2 = g * a1; a3 = g * a2;
  ampVel = 1.f - c.velSens + c.velSens * vel;
  ampVel *= 1.f + accentBoost * .35f;
 }

 void render(const Controls& c, float* outL, float* outR, int n, float noiseLevel) {
  const float gl = std::cos((pan + 1.f) * .7853982f) * 1.41421356f, gr = std::sin((pan + 1.f) * .7853982f) * 1.41421356f;
  const float duty = std::clamp(c.pw + c.pwm * c.lfo * .4f, .05f, .95f);
  const float fmVelScale = 1.f - c.fmVel + c.fmVel * vel;
  const bool osc2Needed = c.l2 > 0.f || c.xmod > 0.f;
  for (int i = 0; i < n; ++i) {
   const float e = amp.next();
   fenvNow = fenv.next();
   // oscillator 2 first: it can modulate oscillator 1
   const float o2 = osc2Needed ? nd::wave(c.wave2, ph2, inc2, duty) : 0.f;
   float i1 = inc1 * (1.f + c.xmod * 1.5f * o2);
   i1 = std::clamp(i1, 0.f, .45f);
   const float o1 = nd::wave(c.wave1, ph1, inc1, duty);
   const float sub = c.sub > 0.f ? nd::pulse(phSub, incSub, .5f) : 0.f;
   ph1 += i1;
   if (ph1 >= 1.f) {
    ph1 -= 1.f;
    if (c.sync) ph2 = ph1 * (inc2 / std::max(1e-6f, inc1)); // hard sync: osc 2 restarts with osc 1
   }
   if (!c.sync || ph1 > 0.f) { ph2 += inc2; if (ph2 >= 1.f) ph2 -= 1.f; }
   phSub += incSub; if (phSub >= 1.f) phSub -= 1.f;
   float x = o1 * c.l1 + o2 * c.l2 + sub * c.sub;
   if (noiseLevel > 0.f) x += rng.next() * noiseLevel;
   if (c.fmLevel > 0.f) {
    fmEnv *= 1.f - c.fmDecayCoef;
    const float index = c.fmIndex * 7.f * (.15f + .85f * fmEnv) * fmVelScale;
    const float m = detail::sine(phMod);
    float pc = phCar + index * m * .15915494f;
    pc -= std::floor(pc);
    x += detail::sine(pc) * c.fmLevel * 1.2f;
    phMod += incMod; if (phMod >= 1.f) phMod -= 1.f;
    phCar += incCar; if (phCar >= 1.f) phCar -= 1.f;
   }
   x *= .5f;
   // drive into the filter
   if (c.drive > 0.f) { const float d = 1.f + c.drive * 5.f; x = detail::softTanh(x * d) / (1.f + c.drive * 2.5f); }
   float y;
   if (c.filterType == 0) {
    // 4-pole ladder, zero-delay feedback solved per sample, saturating input
    const float G2 = G * G, G4 = G2 * G2, one = 1.f - G;
    const float S = G2 * G * s1 * one + G2 * s2 * one + G * s3 * one + s4 * one;
    const float y4 = (G4 * x + S) / (1.f + k * G4);
    float u = x - k * y4;
    u = detail::softTanh(u * 1.2f) / 1.2f;
    auto stage = [&](float in, float& s) { const float v = G * (in - s); const float out = v + s; s = out + v; return out; };
    const float y1 = stage(u, s1), y2 = stage(y1, s2), y3 = stage(y2, s3);
    y = stage(y3, s4) * (1.f + k * .35f);
   } else {
    y = svf.process(x, a1, a2, a3, svfK, c.filterType == 2 ? 1 : c.filterType == 3 ? 2 : 0);
   }
   const float lvl = e * ampVel * uniGain * (1.f - c.lfoAmp * .5f * (1.f - c.lfo) * lfoFade);
   outL[i] += y * lvl * gl;
   outR[i] += y * lvl * gr;
  }
  age += n;
  if (!amp.active()) note = -1;
 }
 float pitchNow() const { return pitch; }
 float vel = 1.f;

private:
 bool wasPlaying() const { return amp.value() > 1e-3f; }
 detail::Random rng;
 float ph1 = 0.f, ph2 = 0.f, phSub = 0.f, phMod = 0.f, phCar = 0.f;
 float inc1 = .01f, inc2 = .01f, incSub = .005f, incMod = .01f, incCar = .01f;
 float pitch = 60.f, target = 60.f, driftNow = 0.f, driftTarget = 0.f, lfoFade = 0.f, fmEnv = 0.f, fenvNow = 0.f, accentBoost = 0.f;
 int driftTicks = 0;
 float g = .1f, G = .1f, k = 0.f, svfK = 1.4f, a1 = 1.f, a2 = 0.f, a3 = 0.f, ampVel = 1.f;
 float s1 = 0.f, s2 = 0.f, s3 = 0.f, s4 = 0.f;
 SVF svf;
 friend class Engine;
};

class Engine {
public:
 static constexpr int maxVoices = 32;

 Engine() { for (int i = 0; i < maxVoices; ++i) voices[static_cast<std::size_t>(i)].seed(static_cast<std::uint32_t>(i + 1)); }

 void prepare(double sampleRate) {
  sr = sampleRate > 0 ? sampleRate : 48000.0;
  for (auto& v : voices) v.kill();
  chorusLine.prepare(static_cast<int>(.03 * sr) + 8);
  delayL.prepare(static_cast<int>(2.2 * sr) + 8); delayR.prepare(static_cast<int>(2.2 * sr) + 8);
  reverb.prepare(sr);
  smoothed = target;
  control();
  countdown = 0;
  held.clear(); held.reserve(64); order.clear(); order.reserve(64); mono.clear(); mono.reserve(64); sustained.clear(); sustained.reserve(128);
  stepPos = 0.0; nextStep = 0; patternRunning = false; pending = {};
 }
 void setValues(const Values& v) { target = v; }
 void setTransport(double bpm, double ppq, bool playing) {
  tempo = bpm > 20.0 ? bpm : 120.0;
  hostPlaying = playing; hostPpq = ppq; resync = playing;
 }
 void setPitchBend(float b) { bend = std::clamp(b, -1.f, 1.f); }
 void setModWheel(float m) { modWheel = std::clamp(m, 0.f, 1.f); }
 void setSustain(bool on) { if (!on && sustain) { sustain = false; for (int n : sustained) releaseNote(n); sustained.clear(); } sustain = on; }
 void setPressure(float) {}

 // ---- MIDI from the keyboard
 void noteOn(int note, float velocity) {
  if (patternMode() != 0) {
   if (held.empty() && smoothed[P::Latch] >= .5f) order.clear(); // a new chord replaces the latched one
   addHeld(note);
   lastVelocity = velocity;
   if (!patternRunning) { patternRunning = true; if (!hostPlaying) { stepPos = 0.0; nextStep = 0; } }
   return;
  }
  playNote(note, velocity, 0.f, false);
 }
 void noteOff(int note) {
  if (patternMode() != 0) {
   held.erase(std::remove(held.begin(), held.end(), note), held.end());
   if (smoothed[P::Latch] < .5f) order.erase(std::remove(order.begin(), order.end(), note), order.end());
   if (order.empty()) { patternRunning = false; stopPatternNote(); }
   return;
  }
  if (sustain) { sustained.push_back(note); return; }
  releaseNote(note);
 }
 void allNotesOff() { for (auto& v : voices) if (v.active()) v.release(); held.clear(); order.clear(); patternRunning = false; mono.clear(); }
 void allSoundOff() {
  for (auto& v : voices) v.kill();
  held.clear(); order.clear(); mono.clear(); patternRunning = false;
  chorusLine.clear(); delayL.clear(); delayR.clear(); reverb.clear();
 }
 int activeVoices() const { int n = 0; for (const auto& v : voices) n += v.active() ? 1 : 0; return n; }
 int currentStep() const { return patternRunning ? displayStep : -1; }

 void render(float* left, float* right, int numSamples) {
  std::fill(left, left + numSamples, 0.f);
  std::fill(right, right + numSamples, 0.f);
  if (resync) { // follow the host's clock
   const double beats = stepBeats();
   stepPos = hostPpq / beats;
   nextStep = static_cast<long long>(std::ceil(stepPos - 1e-9));
   resync = false;
  }
  int done = 0;
  while (done < numSamples) {
   if (countdown <= 0) { control(); countdown = controlInterval; }
   int n = std::min(countdown, numSamples - done);
   if (patternMode() != 0) n = std::min(n, patternEvents(n)); // events land on their sample
   for (auto& v : voices) if (v.active()) v.render(c, left + done, right + done, n, c.noise);
   effects(left + done, right + done, n);
   done += n; countdown -= n;
  }
 }

private:
 // ---------------------------------------------------------------- voices
 int patternMode() const { return smoothed[P::Arp] >= .5f ? 1 : 0; }
 int voiceMode() const { return c.voiceMode; }

 void playNote(int note, float velocity, float accent, bool slide) {
  const int mode = voiceMode();
  const int uni = std::clamp(c.unison, 1, 8);
  if (mode == 0) {
   const float from = c.glideCoef < 1.f && lastPitch >= 0.f ? lastPitch : -1.f;
   for (int u = 0; u < uni; ++u) startVoice(freeVoice(), note, velocity, from, true, accent, u, uni);
   lastPitch = static_cast<float>(note);
   return;
  }
  // mono and legato: one note at a time, the newest wins; legato and slides glide without retrigger
  mono.erase(std::remove(mono.begin(), mono.end(), note), mono.end());
  mono.push_back(note);
  bool sounding = false;
  for (auto& v : voices) if (v.active() && !v.released && v.note >= 0) sounding = true;
  const bool legato = (mode == 2 && sounding) || slide;
  if (legato) {
   int found = 0;
   for (auto& v : voices) if (v.active() && v.note >= 0) { v.glideTo(note, velocity, accent); ++found; }
   if (found > 0) { glideOverride = slide; lastPitch = static_cast<float>(note); return; }
  }
  glideOverride = false;
  const float from = lastPitch >= 0.f && (c.glideCoef < 1.f) ? lastPitch : -1.f;
  int idx = 0;
  for (auto& v : voices) {
   if (!v.active()) continue;
   if (idx < uni) startVoice(v, note, velocity, v.pitchNow(), true, accent, idx, uni), ++idx;
   else v.release();
  }
  for (; idx < uni; ++idx) startVoice(freeVoice(), note, velocity, from, true, accent, idx, uni);
  lastPitch = static_cast<float>(note);
 }
 void releaseNote(int note) {
  const int mode = voiceMode();
  if (mode != 0) {
   mono.erase(std::remove(mono.begin(), mono.end(), note), mono.end());
   if (!mono.empty()) { // fall back to the note still held
    const int back = mono.back();
    for (auto& v : voices) if (v.active() && !v.released) v.glideTo(back, v.vel, 0.f);
    lastPitch = static_cast<float>(back);
    return;
   }
   for (auto& v : voices) if (v.active() && !v.released) v.release();
   return;
  }
  for (auto& v : voices) if (v.note == note && !v.released) v.release();
 }
 void startVoice(Voice& v, int note, float velocity, float from, bool retrigger, float accent, int u, int uni) {
  v.amp.copySettings(ampShape); v.fenv.copySettings(filterShape);
  const float spreadPos = uni > 1 ? static_cast<float>(u) / static_cast<float>(uni - 1) * 2.f - 1.f : 0.f;
  v.detuneSt = spreadPos * c.detune * .35f;          // up to +-35 cents at full detune
  v.pan = spreadPos * c.spread;
  v.uniGain = 1.f / std::sqrt(static_cast<float>(uni));
  v.start(note, velocity, from, retrigger, accent);
  v.control(c, sr);
 }
 Voice& freeVoice() {
  for (auto& v : voices) if (!v.active()) return v;
  Voice* oldest = &voices[0];
  for (auto& v : voices) if (v.released && (!oldest->released || v.age > oldest->age)) oldest = &v;
  if (!oldest->released) for (auto& v : voices) if (v.age > oldest->age) oldest = &v;
  oldest->kill();
  return *oldest;
 }

 // ---------------------------------------------------------------- pattern
 double stepBeats() const {
  static const double beats[6] = {1.0, .5, 1.0 / 3.0, .25, 1.0 / 6.0, .125};
  return beats[std::clamp(static_cast<int>(std::lround(smoothed[P::ArpRate])), 0, 5)];
 }
 void addHeld(int note) {
  if (std::find(held.begin(), held.end(), note) == held.end()) held.push_back(note);
  if (std::find(order.begin(), order.end(), note) == order.end()) order.push_back(note);
 }
 // Plays the pattern's events inside the next n samples; returns how many
 // samples can be rendered before the next event.
 int patternEvents(int n) {
  const double samplesPerStep = stepBeats() * 60.0 / tempo * sr;
  const double swing = smoothed[P::Swing] * .01 * .5;
  // a note's end (gate) inside this stretch?
  if (pending.active) {
   const double until = pending.offAt - stepPos;
   if (until <= 0.0) { stopPatternNote(); }
   else n = std::min(n, std::max(1, static_cast<int>(std::ceil(until * samplesPerStep))));
  }
  if (patternRunning && !order.empty()) {
   const double startAt = static_cast<double>(nextStep) + ((nextStep & 1) ? swing : 0.0);
   const double until = startAt - stepPos;
   if (until <= 1e-9) { triggerStep(); }
   else n = std::min(n, std::max(1, static_cast<int>(std::ceil(until * samplesPerStep))));
  }
  stepPos += static_cast<double>(n) / samplesPerStep;
  return n;
 }
 void triggerStep() {
  const long long step = nextStep++;
  const double gate = std::clamp(smoothed[P::Gate] * .01, .05, 1.0);
  const double swing = smoothed[P::Swing] * .01 * .5;
  const double start = static_cast<double>(step) + ((step & 1) ? swing : 0.0);
  // arpeggiator
  std::vector<int>& src = order;
  const int octaves = std::clamp(static_cast<int>(std::lround(smoothed[P::ArpOctaves])), 1, 4);
  const int mode = static_cast<int>(std::lround(smoothed[P::ArpMode]));
  if (pending.active) stopPatternNote();
  if (mode == 5) { // chord: everything held, on every step
   pending = {true, start + gate, {}, 0};
   for (int nn : src) if (pending.count < 16) { pending.notes[static_cast<std::size_t>(pending.count++)] = nn; playNote(nn, lastVelocity, 0.f, false); }
   displayStep = static_cast<int>(step % 16);
   return;
  }
  std::array<int, 64> seq{};
  int count = 0;
  std::array<int, 16> sorted{};
  int ns = 0;
  for (int nn : src) if (ns < 16) sorted[static_cast<std::size_t>(ns++)] = nn;
  if (mode != 4) std::sort(sorted.begin(), sorted.begin() + ns);
  for (int o = 0; o < octaves; ++o) for (int i = 0; i < ns && count < 64; ++i) seq[static_cast<std::size_t>(count++)] = sorted[static_cast<std::size_t>(i)] + 12 * o;
  if (count == 0) return;
  int index = 0;
  const long long s = step;
  switch (mode) {
   case 1: index = count - 1 - static_cast<int>(s % count); break;                         // down
   case 2: { const int period = std::max(1, 2 * count - 2);                               // up-down
             const int p = static_cast<int>(s % period); index = p < count ? p : period - p; break; }
   case 3: arpRng.state ^= static_cast<std::uint32_t>(s * 2654435761u); index = static_cast<int>((arpRng.next() + 1.f) * .5f * count) % count; break;
   default: index = static_cast<int>(s % count); break;                                    // up, as played
  }
  const int note = std::clamp(seq[static_cast<std::size_t>(index)], 0, 127);
  playNote(note, lastVelocity, 0.f, false);
  pending = {true, start + gate, {note}, 1};
  displayStep = static_cast<int>(step % 16);
 }
 void stopPatternNote() {
  if (!pending.active) return;
  for (int i = 0; i < pending.count; ++i) releaseNote(pending.notes[static_cast<std::size_t>(i)]);
  pending.active = false;
 }

 // ---------------------------------------------------------------- control and effects
 void control() {
  for (std::size_t i = 0; i < smoothed.size(); ++i) {
   if (specs()[i].kind == Kind::Float) smoothed[i] += .3f * (target[i] - smoothed[i]);
   else smoothed[i] = target[i];
  }
  const auto& s = smoothed;
  auto pct = [&](int p) { return s[static_cast<std::size_t>(p)] * .01f; };
  auto choice = [&](int p) { return static_cast<int>(std::lround(s[static_cast<std::size_t>(p)])); };
  c.wave1 = choice(P::Osc1Wave); c.wave2 = choice(P::Osc2Wave);
  c.oct1 = s[P::Osc1Octave]; c.oct2 = s[P::Osc2Octave];
  c.semi2 = s[P::Osc2Semi]; c.fine2 = s[P::Osc2Fine] * .01f;
  c.pw = pct(P::Osc1PW); c.l1 = pct(P::Osc1Level); c.l2 = pct(P::Osc2Level);
  c.sync = s[P::Osc2Sync] >= .5f;
  c.sub = pct(P::SubLevel); c.noise = pct(P::NoiseLevel) * .7f; c.xmod = pct(P::CrossMod); c.pwm = pct(P::PwmDepth);
  static const float ratios[10] = {.5f, 1.f, 2.f, 3.f, 4.f, 5.f, 7.f, 9.f, 11.f, 14.f};
  c.fmLevel = pct(P::FmLevel); c.fmRatio = ratios[std::clamp(choice(P::FmRatio), 0, 9)]; c.fmIndex = pct(P::FmIndex);
  c.fmDecayCoef = 1.f - std::exp(-1.f / (s[P::FmDecay] * .001f * static_cast<float>(sr) * .25f));
  c.fmVel = pct(P::FmVelocity);
  c.filterType = choice(P::FilterType);
  c.cutoff = s[P::Cutoff] * std::exp2(modWheel * 2.f); // the mod wheel opens the filter
  c.res = pct(P::Resonance); c.envAmt = pct(P::FilterEnv); c.keytrack = pct(P::Keytrack); c.filterVel = pct(P::FilterVel); c.drive = pct(P::FilterDrive);
  // LFO
  {
   static const double syncBeats[7] = {0.0, 4.0, 2.0, 1.0, .5, .25, 1.0 / 3.0};
   const int sync = choice(P::LfoSync);
   const double hz = sync > 0 ? tempo / 60.0 / syncBeats[std::clamp(sync, 1, 6)] : static_cast<double>(s[P::LfoRate]);
   if (sync > 0 && hostPlaying) lfoPhase = std::fmod(hostPpq / syncBeats[std::clamp(sync, 1, 6)], 1.0);
   lfoPhase += hz * controlInterval / sr;
   if (lfoPhase >= 1.0) { lfoPhase -= std::floor(lfoPhase); heldRandom = lfoRng.next(); }
   const auto ph = static_cast<float>(lfoPhase);
   switch (choice(P::LfoShape)) {
    case 1: c.lfo = ph < .5f ? 4.f * ph - 1.f : 3.f - 4.f * ph; break;
    case 2: c.lfo = detail::softTanh((ph < .5f ? 1.f : -1.f) * 4.f); break;
    case 3: randomSmooth += .2f * (heldRandom - randomSmooth); c.lfo = randomSmooth; break;
    default: c.lfo = detail::sine(ph); break;
   }
  }
  c.lfoPitch = std::min(1.f, pct(P::LfoPitch) + modWheel * .3f); c.lfoFilter = pct(P::LfoFilter); c.lfoAmp = pct(P::LfoAmp);
  c.lfoFadeStep = s[P::LfoDelay] < 1.f ? 1.f : static_cast<float>(controlInterval / (s[P::LfoDelay] * .001 * sr));
  c.voiceMode = choice(P::VoiceMode); c.unison = choice(P::Unison);
  c.detune = pct(P::Detune); c.spread = pct(P::Spread);
  const float glideMs = glideOverride ? std::max(60.f, s[P::Glide]) : s[P::Glide];
  c.glideCoef = glideMs < 1.f ? 1.f : 1.f - std::exp(-static_cast<float>(controlInterval) / (glideMs * .001f * static_cast<float>(sr) * .25f));
  c.velSens = pct(P::VelSens);
  c.bendSt = bend * s[P::BendRange];
  c.octaveSt = 12.f * s[P::Octave];
  c.drift = pct(P::Drift);
  ampShape.set(s[P::Attack], s[P::Decay], pct(P::Sustain), s[P::Release], sr);
  filterShape.set(s[P::FAttack], s[P::FDecay], pct(P::FSustain), s[P::FRelease], sr);
  for (auto& v : voices) if (v.active()) { v.amp.copySettings(ampShape); v.fenv.copySettings(filterShape); v.control(c, sr); }
  // effects
  chorusMode = choice(P::ChorusMode); chorusMix = pct(P::ChorusMix);
  delayMix = pct(P::DelayMix); delayFb = pct(P::DelayFeedback); pingPong = s[P::DelayPingPong] >= .5f;
  {
   static const double beats[7] = {0.0, 1.0, .5, .75, .25, 2.0 / 3.0, 1.0 / 3.0};
   const int sync = choice(P::DelaySync);
   const double seconds = sync > 0 ? beats[std::clamp(sync, 1, 6)] * 60.0 / tempo : s[P::DelayTime] * .001;
   delaySamplesTarget = static_cast<float>(std::min(seconds, 2.0) * sr);
  }
  delayToneCoef = 1.f - std::exp(-6.2831853f * (1500.f + 14000.f * pct(P::DelayTone) * pct(P::DelayTone)) / static_cast<float>(sr));
  revMix = pct(P::RevMix); revType = choice(P::RevType);
  {
   const float size = revType == 0 ? .35f + .3f * pct(P::RevSize) : pct(P::RevSize);
   const float decay = s[P::RevDecay] * (revType == 0 ? .7f : 1.f);
   const float damp = revType == 0 ? .2f + .4f * pct(P::RevDamp) : pct(P::RevDamp);
   reverb.configure(size, decay, damp, false);
  }
  width = pct(P::Width);
  outTarget = std::pow(10.f, s[P::Output] / 20.f);
 }

 void effects(float* l, float* r, int n) {
  // chorus: one bucket-brigade line, read with opposite LFO phases left and right (as the Juno)
  static const float rates[5] = {0.f, .513f, .863f, 9.75f, .25f};
  static const float depths[5] = {0.f, 1.85f, 1.85f, .3f, .9f}; // ms
  const float cRate = rates[std::clamp(chorusMode, 0, 4)], cDepth = depths[std::clamp(chorusMode, 0, 4)] * .001f * static_cast<float>(sr);
  const float cBase = (chorusMode == 4 ? 6.f : 3.5f) * .001f * static_cast<float>(sr);
  const float revWet = std::sin(revMix * 1.5707963f) * 1.1f, revDry = std::cos(revMix * 1.5707963f);
  for (int i = 0; i < n; ++i) {
   float a = l[i], b = r[i];
   if (chorusMode > 0 || chorusFade > 0.f) {
    chorusFade += (chorusMode > 0 ? 1.f : 0.f) - chorusFade > 0.f ? .002f : -.002f;
    chorusFade = std::clamp(chorusFade, 0.f, 1.f);
    chorusLine.push(.5f * (a + b));
    chorusPhase += cRate / static_cast<float>(sr); if (chorusPhase >= 1.f) chorusPhase -= 1.f;
    const float tri = chorusPhase < .5f ? 4.f * chorusPhase - 1.f : 3.f - 4.f * chorusPhase;
    float wl = chorusLine.read(cBase + cDepth * tri), wr = chorusLine.read(cBase - cDepth * tri);
    if (chorusMode == 4) { // Dimension: two more taps, slower, crossed
     wl = .6f * wl + .4f * chorusLine.read(cBase * 1.7f - cDepth * tri);
     wr = .6f * wr + .4f * chorusLine.read(cBase * 1.7f + cDepth * tri);
    }
    bbdL += .35f * (wl - bbdL); bbdR += .35f * (wr - bbdR); // the BBD's dull top
    const float m = chorusMix * chorusFade;
    a = a * (1.f - .3f * m) + bbdL * .8f * m;
    b = b * (1.f - .3f * m) + bbdR * .8f * m;
   }
   if (delayMix > 0.f || delayTail > 1e-5f) {
    delaySamples += .0005f * (delaySamplesTarget - delaySamples);
    const float dl = delayL.read(delaySamples), dr = delayR.read(delaySamples);
    delayLpL += delayToneCoef * (dl - delayLpL); delayLpR += delayToneCoef * (dr - delayLpR);
    const float inL = pingPong ? .5f * (a + b) : a, inR = pingPong ? 0.f : b;
    delayL.push(detail::softTanh(inL * delayMix + (pingPong ? delayLpR : delayLpL) * delayFb));
    delayR.push(detail::softTanh(inR * delayMix + (pingPong ? delayLpL : delayLpR) * delayFb));
    delayTail = .9995f * delayTail + std::abs(delayLpL) + std::abs(delayLpR);
    a += delayLpL; b += delayLpR;
   }
   if (revMix > 0.f) {
    float rl, rr;
    reverb.process(a, b, rl, rr);
    a = a * revDry + rl * revWet;
    b = b * revDry + rr * revWet;
   }
   // width, then the output with a soft ceiling
   const float mid = .5f * (a + b), side = .5f * (a - b) * width;
   outGain += .002f * (outTarget - outGain);
   l[i] = ceiling((mid + side) * outGain);
   r[i] = ceiling((mid - side) * outGain);
  }
 }

 // untouched below -2 dBFS, then rounded off so nothing passes -0.2 dBFS
 static float ceiling(float x) {
  const float a = std::abs(x);
  if (a <= .8f) return x;
  const float y = .8f + .175f * std::tanh((a - .8f) / .175f);
  return x < 0.f ? -y : y;
 }

 struct Pending { bool active = false; double offAt = 0.0; std::array<int, 16> notes{}; int count = 0; };

 double sr = 48000.0, tempo = 120.0, hostPpq = 0.0, stepPos = 0.0, lfoPhase = 0.0;
 bool hostPlaying = false, resync = false, sustain = false, patternRunning = false, glideOverride = false, pingPong = true;
 long long nextStep = 0;
 int displayStep = -1, countdown = 0, chorusMode = 0, revType = 1;
 float bend = 0.f, modWheel = 0.f, lastVelocity = .8f, lastPitch = -1.f, heldRandom = 0.f, randomSmooth = 0.f;
 float chorusMix = 1.f, chorusPhase = 0.f, chorusFade = 0.f, bbdL = 0.f, bbdR = 0.f;
 float delayMix = 0.f, delayFb = .35f, delaySamples = 18000.f, delaySamplesTarget = 18000.f, delayLpL = 0.f, delayLpR = 0.f, delayToneCoef = .5f, delayTail = 0.f;
 float revMix = .2f, width = 1.f, outGain = .5f, outTarget = .5f;
 std::array<Voice, maxVoices> voices{};
 Controls c;
 Envelope ampShape, filterShape;
 Values target = defaults(), smoothed = defaults();
 std::vector<int> held, order, mono, sustained;
 Pending pending;
 detail::Random lfoRng, arpRng;
 nd::ModDelay chorusLine, delayL, delayR;
 Reverb reverb;
};

} // namespace inverso_abismo::neon
