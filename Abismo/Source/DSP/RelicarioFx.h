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

// Five drive types. Each keeps quiet sounds at about their own level (the
// gain of the curve near zero stays close to 1) and squeezes the loud parts,
// so turning drive up thickens the sound instead of raising its hiss.
class Drive {
public:
 void prepare(double sampleRate) { sr = sampleRate > 0 ? sampleRate : 48000.0; lastAmount = -1.f; }
 void clear() { dc = lp1 = lp2 = bump = env = 0.f; fracture = 1.f; fractureTarget = 1.f; fractureLeft = 0; }
 float process(float x, int type, float amount) {
  if (amount != lastAmount) configure(amount);
  const float k = gain;
  switch (type) {
   case 1: { // fuzz: hard, asymmetric clipping
    float y = x * k * 1.5f;
    y = y > 0.f ? std::min(y, 1.f) : std::max(y, -.55f) * 1.6f;
    return y / n70 * 1.1f;
   }
   case 2: return std::sin(x * k * 1.3f) / n55; // fold
   case 3: { // rectify: the negative half folds up more and more; the note stays
    const float y = detail::softTanh(x * k) / n60;
    const float bend = .25f + .75f * amount; // .5 = half wave, 1 = full wave (octave up)
    float z = y >= 0.f ? y : y * (1.f - 2.f * bend);
    dc += .0015f * (z - dc);
    return (z - dc) * 1.25f;
   }
   case 4: { // shatter: the curve breaks into steps, the sound cracks and dips
    const float y = detail::softTanh(x * k) / n65;
    env = std::max(std::abs(y), env * .9995f);
    const float steps = 3.f + 9.f * (1.f - amount);
    const float stepped = env > 1e-5f ? std::round(y / env * steps) / steps * env : 0.f;
    // fractures: now and then the sound drops back (never to silence) for a moment
    if (fractureLeft > 0) --fractureLeft;
    else if ((rng.next() + 1.f) * .5f < amount * 4.f / static_cast<float>(sr)) {
     fractureLeft = static_cast<int>((.01f + .04f * (rng.next() + 1.f)) * static_cast<float>(sr));
     fractureTarget = .35f + .25f * (rng.next() + 1.f) * .5f;
    }
    fracture += .003f * ((fractureLeft > 0 ? fractureTarget : 1.f) - fracture);
    return (.45f * y + .55f * stepped) * fracture * 1.2f;
   }
   default: { // tape: soft and a little lopsided (even harmonics), a fuller low end,
              // and the top rounding off as it is pushed: warm, not brighter
    const float bias = .3f * amount;
    float y = (detail::softTanh(x * k + bias) - detail::softTanh(bias)) / n80;
    dc += .001f * (y - dc);
    y -= dc;
    bump += bumpCoef * (y - bump);
    y += .3f * amount * bump;
    lp1 += lpCoef * (y - lp1);
    lp2 += lpCoef * (lp1 - lp2);
    return lp2 * 1.1f;
   }
  }
 }
private:
 void configure(float amount) {
  lastAmount = amount;
  gain = 1.f + amount * 7.f;
  n70 = std::pow(gain, .7f); n55 = std::pow(gain, .55f); n60 = std::pow(gain, .6f); n65 = std::pow(gain, .65f); n80 = std::pow(gain, .8f);
  const auto f = [this](float hz) { return 1.f - std::exp(-6.2831853f * hz / static_cast<float>(sr)); };
  lpCoef = f(18000.f * std::pow(.3f, amount)); // 18 kHz clean, about 5 kHz pushed hard
  bumpCoef = f(140.f);
 }
 detail::Random rng;
 double sr = 48000.0;
 float lastAmount = -1.f, lpCoef = 1.f, bumpCoef = .02f, gain = 1.f, n70 = 1.f, n55 = 1.f, n60 = 1.f, n65 = 1.f, n80 = 1.f;
 float dc = 0.f, lp1 = 0.f, lp2 = 0.f, bump = 0.f, env = 0.f, fracture = 1.f, fractureTarget = 1.f;
 int fractureLeft = 0;
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
