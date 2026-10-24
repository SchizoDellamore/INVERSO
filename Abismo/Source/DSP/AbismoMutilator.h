#pragma once
// Post-voice destruction stage: tempo-synced stutter (gate, drop-outs and
// repeats of the previous step), sample-rate reduction and bit crushing.
// The repeat buffer is allocated in prepare(); process() never allocates.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace inverso_abismo {

class Mutilator {
public:
 void prepare(double sampleRate) {
  sr = sampleRate > 0 ? sampleRate : 48000.0;
  // Longest step: 1/4 note at 30 BPM = 2 s; keep two steps of history.
  history.assign(static_cast<std::size_t>(sr * 4.2) * 2, 0.f);
  ramp = static_cast<float>(1.0 - std::exp(-1.0 / (.0015 * sr))); // ~1.5 ms, click-free
  declickCoef = static_cast<float>(std::exp(-1.0 / (.003 * sr)));  // jumps between sources heal over ~3 ms
  envRelease = static_cast<float>(std::exp(-1.0 / (.08 * sr)));
  write = 0; holdCount = 0; heldL = heldR = 0.f; gain = 1.f; lastStep = -1; beat = 0.0;
  offL = offR = 0.f; outL = outR = 0.f; crushEnv = 0.f; dsL1 = dsL2 = dsR1 = dsR2 = 0.f;
 }

 // Forget the recorded history and any gate in progress (no reallocation).
 void clear() {
  std::fill(history.begin(), history.end(), 0.f);
  holdCount = 0; heldL = heldR = 0.f; gain = 1.f; lastStep = -1;
  offL = offR = 0.f; outL = outR = 0.f; crushEnv = 0.f; dsL1 = dsL2 = dsR1 = dsR2 = 0.f;
 }

 // division in beats (0 = stutter off), depth/chaos/crush/downsample in 0..1.
 void configure(double divisionBeats, float depth, float chaosAmount, float crushAmount, float downsampleAmount) {
  division = divisionBeats;
  stutterDepth = std::clamp(depth, 0.f, 1.f);
  chaos = std::clamp(chaosAmount, 0.f, 1.f);
  crush = std::clamp(crushAmount, 0.f, 1.f);
  downsample = std::clamp(downsampleAmount, 0.f, 1.f);
 }

 // Free time: steps last a fixed number of milliseconds instead of following
 // the host's tempo and bar position.
 void setFreeTime(bool isFree, float milliseconds) {
  free = isFree;
  freeMs = std::clamp(milliseconds, 5.f, 4000.f);
 }

 // Probability (0..1) that a step plays the previous step backwards. With the
 // stutter off, reversing still runs on its own 1/8 grid, without the gate.
 void setReverse(float amount) { reverse = std::clamp(amount, 0.f, 1.f); }

 // Percussion: a repeated step is layered over the live signal instead of
 // replacing it, so a hit that lands on a repeat is never swallowed.
 void setLayeredRepeats(bool layered) { layeredRepeats = layered; }

 // Called once per host block. While the host plays, the step grid follows its
 // musical position; stopped, it free-runs at the last known tempo.
 void setTransport(double bpm, double ppq, bool playing) {
  if (std::isfinite(bpm) && bpm >= 20.0 && bpm <= 999.0) tempo = bpm;
  if (!free && playing && std::isfinite(ppq) && std::abs(ppq - beat) > 1e-3) beat = ppq;
 }

 void process(float& l, float& r) {
  const std::size_t frames = history.size() / 2;
  if (frames == 0) return;
  history[write * 2] = l;
  history[write * 2 + 1] = r;
  const bool reverseOnly = division <= 0.0 && reverse > 0.f;
  bool jumped = false;
  if (division > 0.0 || reverseOnly) {
   // In free time a step is freeMs long whatever the tempo (beats advance at the tempo).
   const double stepBeats = free ? static_cast<double>(freeMs) * .001 * tempo / 60.0 : reverseOnly ? .5 : division;
   const auto step = static_cast<long long>(std::floor(beat / stepBeats));
   const double stepSamples = stepBeats * 60.0 / tempo * sr;
   const double phase = beat / stepBeats - static_cast<double>(step);
   if (step != lastStep) {
    lastStep = step;
    const float roll = .5f * (random() + 1.f);
    const float chaosNow = reverseOnly ? 0.f : chaos;
    mode = roll < chaosNow * .45f ? Mode::Drop : roll < chaosNow * .9f ? Mode::Repeat : Mode::Pass;
    if (reverse > 0.f && .5f * (random() + 1.f) < reverse) mode = Mode::Reverse;
    repeatOffset = std::min(static_cast<std::size_t>(stepSamples), frames - 1);
    stepStart = write;
    stepPos = 0;
    jumped = true; // the source may change here (live, repeat, reverse)
   }
   if (mode == Mode::Reverse && repeatOffset > 0) {
    // The previous step, read from its end back to its start.
    const std::size_t back = 1 + std::min(stepPos, repeatOffset - 1);
    const std::size_t read = (stepStart + frames - back) % frames;
    // The start is continuous with the live signal; the end jumps, so fade it out.
    const float tail = std::min(1.f, static_cast<float>(repeatOffset - std::min(stepPos, repeatOffset)) / 96.f);
    l = history[read * 2] * tail;
    r = history[read * 2 + 1] * tail;
   }
   ++stepPos;
   if (mode == Mode::Repeat && repeatOffset > 0) {
    const std::size_t read = (write + frames - repeatOffset) % frames;
    if (layeredRepeats) {
     l += .8f * history[read * 2];
     r += .8f * history[read * 2 + 1];
    } else {
     l = history[read * 2];
     r = history[read * 2 + 1];
    }
   }
   // A jump between sources (a new step, a changed setting) would click: the
   // difference is carried over and faded out in a few milliseconds.
   if (jumped) { offL = outL - l; offR = outR - r; }
   l += offL; r += offR;
   offL *= declickCoef; offR *= declickCoef;
   outL = l; outR = r;
   // Gate: open for the first part of each step; depth shortens the open part.
   const double open = reverseOnly ? 1.0 : 1.0 - .85 * static_cast<double>(stutterDepth);
   float target = phase < open ? 1.f : 0.f;
   if (mode == Mode::Drop) target = 1.f - stutterDepth;
   gain += ramp * (target - gain);
   l *= gain; r *= gain;
   beat += tempo / 60.0 / sr;
  } else {
   // stutter switched off: the last repeat heals into the live signal, the gate fades back in
   if (lastStep >= 0) { offL = outL - l; offR = outR - r; lastStep = -1; }
   l += offL; r += offR;
   offL *= declickCoef; offR *= declickCoef;
   outL = l; outR = r;
   gain += ramp * (1.f - gain);
   l *= gain; r *= gain;
  }
  write = (write + 1) % frames;
  if (downsample > 0.f) {
   const int hold = 1 + static_cast<int>(downsample * downsample * 40.f);
   // a gentle filter at the new rate first: the grit of a low rate, less of its whistle
   const float c = std::min(1.f, 2.4f / static_cast<float>(hold));
   dsL1 += c * (l - dsL1); dsL2 += c * (dsL1 - dsL2);
   dsR1 += c * (r - dsR1); dsR2 += c * (dsR1 - dsR2);
   if (holdCount <= 0) { heldL = dsL2; heldR = dsR2; holdCount = hold; }
   --holdCount;
   l = heldL; r = heldR;
  }
  if (crush > 0.f) {
   // Bits counted against the sound's own level: the grain follows the music
   // and quiet passages do not turn into hiss (12 bits down to 3).
   crushEnv = std::max(std::max(std::abs(l), std::abs(r)), crushEnv * envRelease);
   if (crushEnv > 1e-5f) {
    const float levels = std::exp2(11.f - crush * 9.f) / crushEnv;
    l = std::round(l * levels) / levels;
    r = std::round(r * levels) / levels;
   }
  }
 }

private:
 enum class Mode { Pass, Drop, Repeat, Reverse };
 float random() {
  state ^= state << 13; state ^= state >> 17; state ^= state << 5;
  return static_cast<float>(state) * (2.f / 4294967296.f) - 1.f;
 }
 double sr = 48000.0, tempo = 120.0, beat = 0.0, division = 0.0;
 float ramp = .01f, stutterDepth = .6f, chaos = .3f, crush = 0.f, downsample = 0.f, gain = 1.f, heldL = 0.f, heldR = 0.f;
 std::vector<float> history;
 std::size_t write = 0, repeatOffset = 0, stepStart = 0, stepPos = 0;
 long long lastStep = -1;
 bool layeredRepeats = false;
 float reverse = 0.f, freeMs = 125.f;
 bool free = false;
 int holdCount = 0;
 Mode mode = Mode::Pass;
 float offL = 0.f, offR = 0.f, outL = 0.f, outR = 0.f, declickCoef = .99f;
 float crushEnv = 0.f, envRelease = .9997f, dsL1 = 0.f, dsL2 = 0.f, dsR1 = 0.f, dsR2 = 0.f;
 std::uint32_t state = 0x1234567u;
};

} // namespace inverso_abismo
