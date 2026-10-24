#pragma once
// ABISMO SAMPLER engine. Portable C++17, no JUCE, no allocation in render().
// The sample itself is owned by the caller (plugin or test) and handed over as
// a const pointer; swapping it is the caller's job (see the processor).
// Per voice: classic playback (start, reverse, forward / ping-pong loop with
// crossfade) or a granular cloud (position, scan, size, density, spray, pitch
// and direction randomness), then drive, TPT SVF, formant voice and envelopes.
// Global: LFO, stutter / crush stage, FDN reverb, width, output, soft ceiling.
#include "AbismoCommon.h"
#include "AbismoMutilator.h"
#include "AbismoReverb.h"
#include "SamplerParams.h"
#include <string>
#include <vector>

namespace inverso_abismo::sampler {

struct SampleData {
 std::vector<float> left, right; // right may be empty for mono files
 double sampleRate = 48000.0;
 std::string name;
 int length() const { return static_cast<int>(left.size()); }
};

constexpr int maxVoices = 16;
constexpr int maxGrains = 24;

struct Controls {
 int mode = 0, loopMode = 0, filterMode = 0;
 bool reverse = false;
 float rootNote = 60.f, tune = 0.f, start = 0.f, end = 1.f, loopStart = .2f, loopEnd = .9f, xfade = .2f;
 float position = .3f, scan = 0.f, grainMs = 120.f, density = 30.f, spray = .1f, grainPitchSt = 0.f;
 float grainReverse = 0.f, grainSpread = .5f;
 float cutoff = 20000.f, k = 2.f, filterEnv = 0.f, keytrack = 0.f, drive = 0.f;
 float voiceMix = 0.f, vowel = 0.f, formantRatio = 1.f;
 float pitchEnv = 0.f, pitchTimeMs = 1500.f, driftCents = 0.f, bendRange = 2.f;
 float lfoPitchCents = 0.f, lfoFilterOct = 0.f, lfoAmp = 0.f, lfoPosition = 0.f, velSens = .6f;
};

// 4-point Hermite interpolation; indices outside the sample read as silence.
inline float readSample(const std::vector<float>& data, double index) {
 const int n = static_cast<int>(data.size());
 const auto i = static_cast<int>(std::floor(index));
 const float t = static_cast<float>(index - i);
 auto at = [&](int j) { return (j >= 0 && j < n) ? data[static_cast<std::size_t>(j)] : 0.f; };
 const float xm1 = at(i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);
 const float c1 = .5f * (x1 - xm1);
 const float c2 = xm1 - 2.5f * x0 + 2.f * x1 - .5f * x2;
 const float c3 = .5f * (x2 - xm1) + 1.5f * (x0 - x1);
 return ((c3 * t + c2) * t + c1) * t + x0;
}

class Voice {
public:
 void seed(std::uint32_t s) { rng.state = s * 2654435761u + 777u; }

 void start(int midiNote, float vel, const Controls& c, const SampleData* sample, bool stolen) {
  note = midiNote;
  velocity = std::clamp(vel, 0.f, 1.f);
  released = sustained = false;
  age = 0;
  ended = false;
  endFade = 1.f;
  direction = 1.0;
  const double len = sample ? static_cast<double>(sample->length()) : 0.0;
  playhead = c.start * len;
  cloudCentre = c.position * len;
  scanDirection = 1.0;
  nextGrain = 0.0;
  for (auto& g : grains) g.active = false;
  if (!stolen) { filterL.reset(); filterR.reset(); formant.reset(); driftValue = driftTarget = 0.f; }
  pitchEnvTime = 0.f;
  amp.gateOn();
  fenv.gateOn();
 }
 void release() { released = true; amp.gateOff(); fenv.gateOff(); }
 bool active() const { return amp.active() && !ended; }
 void kill() { amp.reset(); fenv.reset(); }

 void control(const Controls& c, float bendSt, float lfo, float pressure, double sr, const SampleData* sample,
              const Envelope& ampShape, const Envelope& filterShape) {
  amp.copySettings(ampShape);
  fenv.copySettings(filterShape);
  const float dtMs = static_cast<float>(controlInterval * 1000.0 / sr);
  pitchEnvTime += dtMs;
  const float gl = std::clamp(pitchEnvTime / std::max(10.f, c.pitchTimeMs), 0.f, 1.f);
  if (--driftHold <= 0) {
   driftTarget = rng.next();
   driftHold = static_cast<int>((.25 + .375 * (rng.next() + 1.f)) * sr / controlInterval);
  }
  driftValue += .02f * (driftTarget - driftValue);
  const float semis = static_cast<float>(note) - c.rootNote + c.tune + bendSt + c.pitchEnv * gl +
                      driftValue * c.driftCents * .01f + lfo * c.lfoPitchCents * .01f;
  const double sampleRate = sample ? sample->sampleRate : sr;
  rate = static_cast<double>(detail::semitoneRatio(semis)) * sampleRate / sr;
  lfoNow = lfo;

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

 void render(const Controls& c, const SampleData* sample, float* outL, float* outR, int n, double sr) {
  if (!sample || sample->length() < 4) { ended = true; return; }
  const auto& L = sample->left;
  const auto& R = sample->right.empty() ? sample->left : sample->right;
  const double len = static_cast<double>(sample->length());
  const float trem = 1.f - c.lfoAmp * (.5f - .5f * lfoNow);
  const auto fadeSamples = static_cast<float>(.005 * sr);
  for (int i = 0; i < n; ++i) {
   float l = 0.f, r = 0.f;
   if (c.mode == 0) classic(c, L, R, len, l, r, fadeSamples);
   else granular(c, L, R, len, l, r, sample->sampleRate, sr);
   if (ended) break;
   if (c.drive > 0.f) {
    const float pre = 1.f + c.drive * 6.f, post = 1.f / (1.f + c.drive * 2.f);
    l = detail::softTanh(l * pre) * post * 1.8f;
    r = detail::softTanh(r * pre) * post * 1.8f;
   }
   l = filterL.process(l, a1, a2, a3, k, c.filterMode);
   r = filterR.process(r, a1, a2, a3, k, c.filterMode);
   if (c.voiceMix > 0.f) formant.process(l, r, c.voiceMix);
   const float e = amp.next() * ampVel * trem * .5f;
   fenv.next();
   outL[i] += l * e;
   outR[i] += r * e;
  }
  age += n;
 }

 // Normalised read position for the editor's playhead marker.
 float displayPosition(double len) const {
  if (len <= 0.0) return 0.f;
  return static_cast<float>(std::clamp((mode == 0 ? playhead : cloudCentre) / len, 0.0, 1.0));
 }

 int note = -1;
 bool released = false, sustained = false;
 long long age = 0;
 Envelope amp, fenv;
 int mode = 0;

private:
 struct Grain {
  bool active = false;
  double pos = 0.0, step = 1.0;
  int length = 1, elapsed = 0;
  float gainL = 1.f, gainR = 1.f;
 };

 // Mirrored index when reversed: loop points then refer to the reversed sound.
 static double physical(double virtualIndex, double len, bool reversed) {
  return reversed ? len - 1.0 - virtualIndex : virtualIndex;
 }

 void classic(const Controls& c, const std::vector<float>& L, const std::vector<float>& R, double len,
              float& l, float& r, float fadeSamples) {
  mode = 0;
  const double loopStart = std::clamp(static_cast<double>(std::min(c.loopStart, c.loopEnd)) * len, 0.0, len - 2.0);
  const double loopEnd = std::clamp(static_cast<double>(std::max(c.loopStart, c.loopEnd)) * len, loopStart + 1.0, len - 1.0);
  const double loopLen = loopEnd - loopStart;
  const bool looping = c.loopMode != 0 && loopLen >= 16.0;
  auto read = [&](double v, float& outL, float& outR) {
   const double p = physical(v, len, c.reverse);
   outL = readSample(L, p);
   outR = readSample(R, p);
  };
  read(playhead, l, r);
  if (looping && c.loopMode == 1) {
   const double fadeLen = std::max(1.0, static_cast<double>(c.xfade) * .5 * loopLen);
   if (playhead > loopEnd - fadeLen && playhead < loopEnd) {
    const float t = static_cast<float>((playhead - (loopEnd - fadeLen)) / fadeLen);
    float pl, pr;
    read(playhead - loopLen, pl, pr);
    l += t * (pl - l);
    r += t * (pr - r);
   }
  }
  playhead += rate * direction;
  if (looping && c.loopMode == 1) {
   if (playhead >= loopEnd) playhead -= loopLen;
  } else if (looping && c.loopMode == 2) {
   if (playhead >= loopEnd && playhead > loopStart) { playhead = loopEnd - (playhead - loopEnd); direction = -1.0; }
   else if (playhead <= loopStart && direction < 0.0) { playhead = loopStart + (loopStart - playhead); direction = 1.0; }
  } else {
   // One-shot: short fade at END (the region's end), then free the voice.
   const double stop = std::clamp(static_cast<double>(std::max(c.end, c.start + .001f)) * len, 1.0, len - 1.0);
   const double remaining = (stop - playhead) / std::max(1e-6, rate);
   if (remaining < static_cast<double>(fadeSamples)) endFade = std::max(0.f, static_cast<float>(remaining) / fadeSamples);
   if (playhead >= stop) { ended = true; amp.reset(); fenv.reset(); }
  }
  l *= endFade;
  r *= endFade;
 }

 void granular(const Controls& c, const std::vector<float>& L, const std::vector<float>& R, double len,
               float& l, float& r, double sampleRate, double sr) {
  mode = 1;
  // The cloud centre travels at SCAN x real time, independent of pitch, inside the region
  // (START..END). LOOP decides how: Off runs round the whole region; Forward goes round the loop
  // (LOOP START..LOOP END) once it reaches it; Ping-pong goes back and forth inside the loop.
  const double lo = std::clamp(static_cast<double>(std::min(c.start, c.end)) * len, 0.0, len - 2.0);
  const double hi = std::clamp(static_cast<double>(std::max(c.start, c.end)) * len, lo + 1.0, len - 1.0);
  const double loopLo = std::clamp(static_cast<double>(std::min(c.loopStart, c.loopEnd)) * len, lo, hi - 1.0);
  const double loopHi = std::clamp(static_cast<double>(std::max(c.loopStart, c.loopEnd)) * len, loopLo + 1.0, hi);
  const bool loopUsable = c.loopMode != 0 && loopHi - loopLo >= 16.0;
  // a cloud outside the region (POSITION before START, region moved) starts at its nearest edge
  if (cloudCentre < lo || cloudCentre > hi) cloudCentre = std::clamp(cloudCentre, lo, hi);
  const bool pingPong = loopUsable && c.loopMode == 2;
  if (pingPong) { // outside the loop the cloud heads for it
   if (cloudCentre < loopLo) scanDirection = c.scan >= 0.f ? 1.0 : -1.0;
   else if (cloudCentre > loopHi) scanDirection = c.scan >= 0.f ? -1.0 : 1.0;
  } else scanDirection = 1.0;
  const double before = cloudCentre;
  cloudCentre += static_cast<double>(c.scan) * sampleRate / sr * scanDirection;
  if (pingPong) { // inside, it turns round at each end
   if (before <= loopHi && cloudCentre > loopHi) { cloudCentre = 2.0 * loopHi - cloudCentre; scanDirection = -scanDirection; }
   else if (before >= loopLo && cloudCentre < loopLo) { cloudCentre = 2.0 * loopLo - cloudCentre; scanDirection = -scanDirection; }
  } else if (loopUsable) { // forward: round the loop once it gets there
   const double span = loopHi - loopLo;
   if (c.scan >= 0.f && before < loopHi && cloudCentre >= loopHi) cloudCentre -= span;
   else if (c.scan < 0.f && before > loopLo && cloudCentre <= loopLo) cloudCentre += span;
   const double region = hi - lo; // from outside the loop it comes round the region to it
   while (cloudCentre >= hi) cloudCentre -= region;
   while (cloudCentre < lo) cloudCentre += region;
  } else { // off: round the whole region
   const double span = hi - lo;
   while (cloudCentre >= hi) cloudCentre -= span;
   while (cloudCentre < lo) cloudCentre += span;
  }
  cloudCentre = std::clamp(cloudCentre, lo, hi);
  if (nextGrain <= 0.0) {
   spawn(c, len, sampleRate, sr, lo, hi);
   const double interval = sr / std::max(1.f, c.density);
   nextGrain += interval * (1.0 + .3 * static_cast<double>(rng.next()));
  }
  nextGrain -= 1.0;
  const float norm = 1.f / std::sqrt(std::max(1.f, c.density * c.grainMs * .001f));
  for (auto& g : grains) {
   if (!g.active) continue;
   const float t = static_cast<float>(g.elapsed) / static_cast<float>(g.length);
   const float w = detail::sine(.5f * t);
   const float window = w * w * norm * 1.6f;
   const float sl = readSample(L, g.pos), sr2 = readSample(R, g.pos);
   l += sl * window * g.gainL;
   r += sr2 * window * g.gainR;
   g.pos += g.step;
   if (++g.elapsed >= g.length) g.active = false;
  }
 }

 void spawn(const Controls& c, double len, double sampleRate, double sr, double lo, double hi) {
  Grain* slot = nullptr;
  for (auto& g : grains) if (!g.active) { slot = &g; break; }
  if (!slot) return;
  const double centre = cloudCentre + static_cast<double>(c.lfoPosition) * .25 * len * lfoNow;
  const double sprayRange = static_cast<double>(c.spray) * .5 * sampleRate; // up to +-500 ms
  double pos = centre + sprayRange * static_cast<double>(rng.next());
  // grains come from the region only
  const double span = std::max(1.0, hi - lo);
  pos = lo + std::fmod(std::fmod(pos - lo, span) + span, span);
  const float pitch = c.grainPitchSt * rng.next();
  const bool backwards = (.5f * (rng.next() + 1.f)) < c.grainReverse;
  const double step = rate * static_cast<double>(detail::semitoneRatio(pitch)) * (c.reverse != backwards ? -1.0 : 1.0);
  slot->active = true;
  slot->pos = c.reverse ? len - 1.0 - pos : pos;
  slot->step = step;
  slot->length = std::max(8, static_cast<int>(c.grainMs * .001f * static_cast<float>(sr)));
  // nothing outside START..END is heard: a grain that would run past the region's edge starts
  // earlier so it fits whole; only a region shorter than the grain shortens it
  const double physLo = c.reverse ? len - 1.0 - hi : lo, physHi = c.reverse ? len - 1.0 - lo : hi; // REVERSE mirrors the file
  const double travel = static_cast<double>(slot->length) * std::abs(step);
  if (travel >= physHi - physLo) {
   slot->length = std::max(8, static_cast<int>((physHi - physLo) / std::max(1e-6, std::abs(step))));
   slot->pos = step >= 0.0 ? physLo : physHi;
  } else if (step >= 0.0) slot->pos = std::min(slot->pos, physHi - travel);
  else slot->pos = std::max(slot->pos, physLo + travel);
  slot->elapsed = 0;
  const float pan = .5f + .5f * c.grainSpread * rng.next();
  slot->gainL = std::cos(pan * 1.5707963f) * 1.4142f;
  slot->gainR = std::sin(pan * 1.5707963f) * 1.4142f;
 }

 std::array<Grain, maxGrains> grains{};
 SVF filterL, filterR;
 FormantFilter formant;
 detail::Random rng;
 double playhead = 0.0, direction = 1.0, rate = 1.0, cloudCentre = 0.0, nextGrain = 0.0, scanDirection = 1.0;
 int driftHold = 0;
 bool ended = false;
 float velocity = 1.f, pitchEnvTime = 0.f, driftValue = 0.f, driftTarget = 0.f, ampVel = 1.f, endFade = 1.f;
 float lfoNow = 0.f, a1 = 1.f, a2 = 0.f, a3 = 0.f, k = 2.f;
};

class Engine {
public:
 void prepare(double sampleRate) {
  sr = (std::isfinite(sampleRate) && sampleRate >= 8000.0) ? sampleRate : 48000.0;
  reverb.prepare(sr);
  mutilator.prepare(sr);
  resampler.prepare(sr);
  for (int i = 0; i < maxVoices; ++i) {
   voices[static_cast<std::size_t>(i)].seed(static_cast<std::uint32_t>(i + 101));
   voices[static_cast<std::size_t>(i)].kill();
  }
  smoothed = target;
  controlCountdown = 0;
  lfoPhase = 0.f;
  derive();
 }
 void setValues(const Values& v) { target = v; }

 // The pointer must stay valid until another sample is set. Voices are cut
 // immediately: they must never read the previous sample's memory again.
 void setSample(const SampleData* s) {
  if (s == sample) return;
  for (auto& v : voices) v.kill();
  sample = s;
 }
 const SampleData* currentSample() const { return sample; }

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
  chosen->start(note, velocity, controls, sample, stolen);
  chosen->control(controls, bend * controls.bendRange, lfoValue, pressure, sr, sample, ampShape, filterShape);
  lastVoice = chosen;
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
 void allSoundOff() { for (auto& v : voices) v.kill(); reverb.clear(); }
 void setPitchBend(float normalised) { bend = std::clamp(normalised, -1.f, 1.f); }
 void setModWheel(float value) { modWheel = std::clamp(value, 0.f, 1.f); }
 void setPressure(float value) { pressure = std::clamp(value, 0.f, 1.f); }
 void setTransport(double bpm, double ppq, bool playing) { mutilator.setTransport(bpm, ppq, playing); }

 int activeVoices() const {
  int n = 0;
  for (const auto& v : voices) n += v.active() ? 1 : 0;
  return n;
 }
 // Playhead of the most recently started voice, 0..1, or -1 when silent.
 float playheadPosition() const {
  if (!lastVoice || !lastVoice->active() || !sample) return -1.f;
  return lastVoice->displayPosition(static_cast<double>(sample->length()));
 }

 void render(float* left, float* right, int numSamples) {
  int done = 0;
  while (done < numSamples) {
   if (controlCountdown <= 0) { controlTick(); controlCountdown = controlInterval; }
   const int n = std::min(controlCountdown, numSamples - done);
   float* l = left + done;
   float* r = right + done;
   std::fill(l, l + n, 0.f);
   std::fill(r, r + n, 0.f);
   for (auto& v : voices) if (v.active()) v.render(controls, sample, l, r, n, sr);
   const float mix = smoothed[P::RevMix] * .01f;
   const float dryGain = std::cos(mix * 1.5707963f), wetGain = std::sin(mix * 1.5707963f) * 1.4f;
   const float width = smoothed[P::Width] * .01f;
   for (int i = 0; i < n; ++i) {
    float wl = 0.f, wr = 0.f;
    if (resampler.enabled()) resampler.process(l[i], r[i]);
    mutilator.process(l[i], r[i]);
    reverb.process(l[i], r[i], wl, wr);
    float ol = l[i] * dryGain + wl * wetGain;
    float orr = r[i] * dryGain + wr * wetGain;
    const float mid = .5f * (ol + orr), side = .5f * (ol - orr) * width;
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
  lfoPhase += static_cast<float>(smoothed[P::LfoRate] * controlInterval / sr);
  while (lfoPhase >= 1.f) lfoPhase -= 1.f;
  lfoValue = detail::sine(lfoPhase);
  for (auto& v : voices)
   if (v.active()) v.control(controls, bend * controls.bendRange, lfoValue, pressure, sr, sample, ampShape, filterShape);
 }

 void derive() {
  const auto& s = smoothed;
  auto pct = [&](int p) { return s[static_cast<std::size_t>(p)] * .01f; };
  auto choice = [&](int p) { return static_cast<int>(std::lround(s[static_cast<std::size_t>(p)])); };
  const float tension = std::clamp(pct(P::Tension) + modWheel, 0.f, 1.f);
  Controls& c = controls;
  c.mode = choice(P::Mode);
  c.rootNote = s[P::RootNote];
  c.tune = s[P::Tune];
  c.start = pct(P::Start);
  c.end = pct(P::End);
  c.reverse = s[P::Reverse] >= .5f;
  c.loopMode = choice(P::LoopMode);
  c.loopStart = pct(P::LoopStart);
  c.loopEnd = pct(P::LoopEnd);
  c.xfade = pct(P::Xfade);
  c.position = pct(P::Position);
  c.scan = pct(P::Scan);
  c.grainMs = s[P::GrainSize];
  c.density = s[P::Density];
  c.spray = std::min(1.f, pct(P::Spray) + tension * .4f);
  const float gp = std::min(1.f, pct(P::GrainPitch) + tension * .3f);
  c.grainPitchSt = gp * gp * 12.f; // up to +-12 semitones per grain
  c.grainReverse = pct(P::GrainReverse);
  c.grainSpread = pct(P::GrainSpread);
  c.filterMode = choice(P::FilterMode);
  c.cutoff = s[P::Cutoff];
  c.k = 2.f * (1.f - .97f * std::min(1.f, pct(P::Resonance) + tension * .25f));
  c.filterEnv = pct(P::FilterEnv);
  c.keytrack = pct(P::Keytrack);
  c.drive = pct(P::Drive);
  c.voiceMix = pct(P::VoiceMix);
  c.vowel = pct(P::Vowel);
  c.formantRatio = detail::semitoneRatio(s[P::FormantShift]);
  c.velSens = pct(P::VelSens);
  c.pitchEnv = s[P::PitchEnv];
  c.pitchTimeMs = s[P::PitchTime];
  const float drift = std::min(1.f, pct(P::Drift) + tension * .6f);
  c.driftCents = drift * drift * 60.f;
  c.bendRange = s[P::BendRange];
  const float lp = std::min(1.f, pct(P::LfoPitch) + tension * .15f);
  c.lfoPitchCents = lp * lp * 200.f;
  c.lfoFilterOct = pct(P::LfoFilter) * 3.f;
  c.lfoAmp = pct(P::LfoAmp);
  c.lfoPosition = pct(P::LfoPosition);
  static const double divisions[7] = {0.0, 1.0, .5, .25, .125, 1.0 / 3.0, 1.0 / 6.0};
  mutilator.configure(divisions[std::clamp(choice(P::StutterDiv), 0, 6)], pct(P::StutterDepth),
                      std::min(1.f, pct(P::Chaos) + tension * .3f), 0.f, 0.f);
  resampler.set(s[P::ResampleOn] >= .5f ? choice(P::Resample) : 0, s[P::ResampleOn] >= .5f ? pct(P::ResampleSat) : 0.f);
  mutilator.setFreeTime(choice(P::StutterSync) == 1, s[P::StutterMs]);
  ampShape.set(s[P::Attack], s[P::Decay], pct(P::Sustain), s[P::Release], sr);
  filterShape.set(s[P::FAttack], s[P::FDecay], pct(P::FSustain), s[P::FRelease], sr);
  reverb.configure(pct(P::RevSize), s[P::RevDecay], pct(P::RevDamp), s[P::Freeze] >= .5f);
  outGain = std::pow(10.f, s[P::Output] / 20.f);
 }

 double sr = 48000.0;
 Values target = defaults(), smoothed = defaults();
 Controls controls;
 Envelope ampShape, filterShape;
 std::array<Voice, maxVoices> voices{};
 const Voice* lastVoice = nullptr;
 const SampleData* sample = nullptr;
 Reverb reverb;
 Mutilator mutilator;
 Resampler resampler;
 int controlCountdown = 0;
 float lfoPhase = 0.f, lfoValue = 0.f, bend = 0.f, modWheel = 0.f, pressure = 0.f, outGain = 1.f;
 bool sustainPedal = false;
};

} // namespace inverso_abismo::sampler
