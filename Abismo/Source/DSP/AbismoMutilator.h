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
  write = 0; holdCount = 0; heldL = heldR = 0.f; gain = 1.f; lastStep = -1; beat = 0.0;
 }

 // division in beats (0 = stutter off), depth/chaos/crush/downsample in 0..1.
 void configure(double divisionBeats, float depth, float chaosAmount, float crushAmount, float downsampleAmount) {
  division = divisionBeats;
  stutterDepth = std::clamp(depth, 0.f, 1.f);
  chaos = std::clamp(chaosAmount, 0.f, 1.f);
  crush = std::clamp(crushAmount, 0.f, 1.f);
  downsample = std::clamp(downsampleAmount, 0.f, 1.f);
 }

 // Called once per host block. While the host plays, the step grid follows its
 // musical position; stopped, it free-runs at the last known tempo.
 void setTransport(double bpm, double ppq, bool playing) {
  if (std::isfinite(bpm) && bpm >= 20.0 && bpm <= 999.0) tempo = bpm;
  if (playing && std::isfinite(ppq) && std::abs(ppq - beat) > 1e-3) beat = ppq;
 }

 void process(float& l, float& r) {
  const std::size_t frames = history.size() / 2;
  if (frames == 0) return;
  history[write * 2] = l;
  history[write * 2 + 1] = r;
  if (division > 0.0) {
   const double stepBeats = division;
   const auto step = static_cast<long long>(std::floor(beat / stepBeats));
   const double stepSamples = stepBeats * 60.0 / tempo * sr;
   const double phase = beat / stepBeats - static_cast<double>(step);
   if (step != lastStep) {
    lastStep = step;
    const float roll = .5f * (random() + 1.f);
    mode = roll < chaos * .45f ? Mode::Drop : roll < chaos * .9f ? Mode::Repeat : Mode::Pass;
    repeatOffset = std::min(static_cast<std::size_t>(stepSamples), frames - 1);
   }
   if (mode == Mode::Repeat && repeatOffset > 0) {
    const std::size_t read = (write + frames - repeatOffset) % frames;
    l = history[read * 2];
    r = history[read * 2 + 1];
   }
   // Gate: open for the first part of each step; depth shortens the open part.
   const double open = 1.0 - .85 * static_cast<double>(stutterDepth);
   float target = phase < open ? 1.f : 0.f;
   if (mode == Mode::Drop) target = 1.f - stutterDepth;
   gain += ramp * (target - gain);
   l *= gain; r *= gain;
   beat += tempo / 60.0 / sr;
  } else {
   gain += ramp * (1.f - gain); // stutter switched off mid-gate: fade back in
   l *= gain; r *= gain;
  }
  write = (write + 1) % frames;
  if (downsample > 0.f) {
   const int hold = 1 + static_cast<int>(downsample * downsample * 40.f);
   if (holdCount <= 0) { heldL = l; heldR = r; holdCount = hold; }
   --holdCount;
   l = heldL; r = heldR;
  }
  if (crush > 0.f) {
   const float levels = std::exp2(15.f - crush * 13.f); // 16 bits down to 3 bits
   l = std::round(l * levels) / levels;
   r = std::round(r * levels) / levels;
  }
 }

private:
 enum class Mode { Pass, Drop, Repeat };
 float random() {
  state ^= state << 13; state ^= state >> 17; state ^= state << 5;
  return static_cast<float>(state) * (2.f / 4294967296.f) - 1.f;
 }
 double sr = 48000.0, tempo = 120.0, beat = 0.0, division = 0.0;
 float ramp = .01f, stutterDepth = .6f, chaos = .3f, crush = 0.f, downsample = 0.f, gain = 1.f, heldL = 0.f, heldR = 0.f;
 std::vector<float> history;
 std::size_t write = 0, repeatOffset = 0;
 long long lastStep = -1;
 int holdCount = 0;
 Mode mode = Mode::Pass;
 std::uint32_t state = 0x1234567u;
};

} // namespace inverso_abismo
