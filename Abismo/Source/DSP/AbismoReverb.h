#pragma once
// 8-line feedback delay network with Householder mixing, in-loop damping and
// slow delay modulation. Buffers are allocated only in prepare(); process()
// never allocates. FREEZE makes the loop lossless and mutes the input.
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace inverso_abismo {

class Reverb {
public:
 void prepare(double sampleRate) {
  sr = sampleRate > 0 ? sampleRate : 48000.0;
  const auto maxLen = static_cast<std::size_t>(sr * 0.22) + 64;
  for (auto& l : lines) { l.buffer.assign(maxLen, 0.f); l.write = 0; l.lowpass = 0.f; }
  const double diffusionMs[4] = {4.7, 3.6, 12.7, 9.3};
  for (std::size_t i = 0; i < diffusers.size(); ++i) {
   for (std::size_t c = 0; c < 2; ++c) {
    auto& d = diffusers[i][c];
    // Slightly different right-channel lengths decorrelate the stereo image.
    d.buffer.assign(static_cast<std::size_t>(sr * diffusionMs[i] * (c ? .00107 : .001)) + 1, 0.f);
    d.index = 0;
   }
  }
  for (std::size_t i = 0; i < lines.size(); ++i) {
   lines[i].length = targetLength(i, size);
   lines[i].phase = static_cast<float>(i) * .37f;
  }
  configure(size, decaySeconds, damping, frozen);
  updateGains();
 }

 void clear() {
  for (auto& l : lines) { std::fill(l.buffer.begin(), l.buffer.end(), 0.f); l.lowpass = 0.f; }
  for (auto& set : diffusers) for (auto& d : set) std::fill(d.buffer.begin(), d.buffer.end(), 0.f);
 }

 // size, damp in 0..1, decay in seconds (RT60).
 void configure(float newSize, float newDecay, float newDamp, bool freeze) {
  size = std::clamp(newSize, 0.f, 1.f);
  decaySeconds = std::clamp(newDecay, .1f, 60.f);
  damping = std::clamp(newDamp, 0.f, 1.f);
  frozen = freeze;
 }

 void process(float inL, float inR, float& outL, float& outR) {
  if (lines[0].buffer.empty()) { outL = outR = 0.f; return; }
  const float inputGain = frozen ? 0.f : 1.f;
  if (++gainCounter >= 32) { gainCounter = 0; updateGains(); }
  float l = diffuse(0, inL * inputGain), r = diffuse(1, inR * inputGain);
  std::array<float, 8> y{};
  float sum = 0.f;
  const float dampCoef = frozen ? 1.f : 1.f - damping * .88f;
  for (std::size_t i = 0; i < lines.size(); ++i) {
   auto& line = lines[i];
   // Glide the delay length toward its target so SIZE changes never click.
   line.length += .0005f * (targetLength(i, size) - line.length);
   line.phase += static_cast<float>((0.07 + 0.031 * static_cast<double>(i)) / sr);
   if (line.phase >= 1.f) line.phase -= 1.f;
   const float mod = static_cast<float>(sr * .0004) * std::sin(6.2831853f * line.phase);
   const float readDelay = std::clamp(line.length + mod, 2.f, static_cast<float>(line.buffer.size() - 3));
   float out = read(line, readDelay);
   line.lowpass += dampCoef * (out - line.lowpass);
   y[i] = line.lowpass;
   sum += y[i];
  }
  const float householder = sum * .25f; // 2/N with N = 8
  for (std::size_t i = 0; i < lines.size(); ++i) {
   auto& line = lines[i];
   const float gain = frozen ? 1.f : gains[i];
   const float input = (i % 2 == 0 ? l : r) * .35f;
   float v = input + (y[i] - householder) * gain;
   if (!std::isfinite(v)) v = 0.f;
   line.buffer[line.write] = v;
   line.write = (line.write + 1) % line.buffer.size();
  }
  outL = (y[0] - y[2] + y[4] - y[6] + .5f * (y[1] - y[7])) * .45f;
  outR = (y[1] - y[3] + y[5] - y[7] + .5f * (y[0] - y[6])) * .45f;
 }

private:
 void updateGains() {
  for (std::size_t i = 0; i < lines.size(); ++i)
   gains[i] = static_cast<float>(std::pow(10.0, -3.0 * lines[i].length / (decaySeconds * sr)));
 }
 struct Line { std::vector<float> buffer; std::size_t write = 0; float length = 100.f, lowpass = 0.f, phase = 0.f; };
 struct Allpass { std::vector<float> buffer; std::size_t index = 0; };

 float targetLength(std::size_t i, float s) const {
  static const double baseMs[8] = {37.1, 43.7, 53.3, 61.9, 71.3, 83.1, 97.7, 113.9};
  return static_cast<float>(baseMs[i] * .001 * sr * (.35 + 1.25 * static_cast<double>(s)));
 }
 static float read(const Line& line, float delay) {
  const auto n = line.buffer.size();
  const float pos = static_cast<float>(line.write) + static_cast<float>(n) - delay;
  const auto i0 = static_cast<std::size_t>(pos) % n;
  const float frac = pos - std::floor(pos);
  return line.buffer[i0] + frac * (line.buffer[(i0 + 1) % n] - line.buffer[i0]);
 }
 float diffuse(int channel, float x) {
  for (auto& set : diffusers) {
   auto& d = set[static_cast<std::size_t>(channel)];
   const float delayed = d.buffer[d.index];
   const float v = x + .6f * delayed;
   d.buffer[d.index] = v;
   d.index = (d.index + 1) % d.buffer.size();
   x = delayed - .6f * v;
  }
  return x;
 }

 double sr = 48000.0;
 float size = .7f, decaySeconds = 6.f, damping = .45f;
 bool frozen = false;
 int gainCounter = 0;
 std::array<float, 8> gains{};
 std::array<Line, 8> lines;
 std::array<std::array<Allpass, 2>, 4> diffusers;
};

} // namespace inverso_abismo
