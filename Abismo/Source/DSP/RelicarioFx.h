#pragma once
// RELICARIO effects rack: drive, phaser, flanger and the delay line shared by
// the chorus, flanger and tape echo. Portable, no JUCE.
#include "AbismoCommon.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace inverso_abismo::relicario {

// A modulated delay line (linear interpolation).
class DelayLine {
public:
 void prepare(int frames) { buffer.assign(static_cast<std::size_t>(std::max(4, frames)), 0.f); write = 0; }
 void clear() { std::fill(buffer.begin(), buffer.end(), 0.f); }
 void push(float x) { buffer[static_cast<std::size_t>(write)] = x; write = (write + 1) % static_cast<int>(buffer.size()); }
 float read(float delaySamples) const {
  const int size = static_cast<int>(buffer.size());
  const float d = std::clamp(delaySamples, 1.f, static_cast<float>(size - 2));
  const float pos = static_cast<float>(write) - d + static_cast<float>(size);
  const int i0 = static_cast<int>(pos) % size, i1 = (i0 + 1) % size;
  const float f = pos - std::floor(pos);
  return buffer[static_cast<std::size_t>(i0)] + f * (buffer[static_cast<std::size_t>(i1)] - buffer[static_cast<std::size_t>(i0)]);
 }
private:
 std::vector<float> buffer;
 int write = 0;
};

// Five drive types, all at roughly the same loudness as the tape drive.
class Drive {
public:
 void clear() { dc = 0.f; hold = 0; }
 float process(float x, int type, float amount) {
  const float pre = 1.f + amount * 8.f, post = 1.f / (1.f + amount * 2.5f);
  switch (type) {
   case 1: { // fuzz: hard, asymmetric clipping
    float y = x * pre * 1.5f;
    y = y > 0.f ? std::min(y, 1.f) : std::max(y, -.55f) * 1.6f;
    return y * post * 1.6f;
   }
   case 2: return std::sin(x * pre * 1.3f) * (.55f + .45f * post); // fold
   case 3: { // rectify: an octave up and buzzing, DC removed
    const float y = detail::softTanh(std::abs(x) * pre) * 1.6f;
    dc += .0015f * (y - dc);
    return (y - dc) * (.5f + post);
   }
   case 4: { // shatter: crushed curve with dropouts that crackle
    if (hold > 0) { --hold; return 0.f; }
    if ((rng.next() + 1.f) * .5f < amount * .0015f) hold = 8 + static_cast<int>((rng.next() + 1.f) * 120.f);
    const float y = x * pre;
    const float a = std::min(1.f, std::abs(y));
    return (y < 0.f ? -1.f : 1.f) * std::sqrt(std::sqrt(a)) * (.45f + .4f * post);
   }
   default: return detail::softTanh(x * pre) * post * 1.8f; // tape
  }
 }
private:
 detail::Random rng;
 float dc = 0.f;
 int hold = 0;
};

// Six first-order all-pass stages swept by an LFO, with feedback.
class Phaser {
public:
 void clear() { stages.fill(0.f); last = 0.f; }
 // coef: all-pass coefficient for this sample (from the sweep), fb 0..0.95.
 float process(float x, float coef, float fb) {
  float y = x + last * fb;
  for (auto& z : stages) {
   const float out = coef * y + z;
   z = y - coef * out;
   y = out;
  }
  last = y;
  return y;
 }
 static float coefFor(float hz, double sr) {
  const float t = std::tan(3.14159265f * std::min(hz, static_cast<float>(sr) * .45f) / static_cast<float>(sr));
  return (t - 1.f) / (t + 1.f);
 }
private:
 std::array<float, 6> stages{};
 float last = 0.f;
};

} // namespace inverso_abismo::relicario
