#pragma once
// RESAMPLE: the sound of rendering a track down to a low sample rate offline and
// back up (the lo-fi, "underwater" top end of an old sampler), in real time.
// A steep 8-pole low pass at the new Nyquist, a sample-and-hold at the chosen
// rate, then the same steep low pass again to take the steps away. What is left
// is the loss of the highs with a little of the fold-back of a real converter
// near the edge. Not a bit crusher: the level and the body of the sound stay.
// Optional saturation adds weight. Changing the rate crossfades (no clicks).
// Portable (no JUCE).
#include <algorithm>
#include <array>
#include <cmath>

namespace inverso_abismo {

class Resampler {
public:
 static constexpr int numRates = 8;
 // "Off|20000 Hz|10000 Hz|7500 Hz|5000 Hz|2500 Hz|1000 Hz|500 Hz"
 static float rateOf(int choice) {
  static const float rates[numRates] = {0.f, 20000.f, 10000.f, 7500.f, 5000.f, 2500.f, 1000.f, 500.f};
  return rates[std::clamp(choice, 0, numRates - 1)];
 }
 static const char* choices() { return "Off|20000 Hz|10000 Hz|7500 Hz|5000 Hz|2500 Hz|1000 Hz|500 Hz"; }
 // For sounds made with the old bit crush / downsample: the nearest rate.
 static int choiceForCrush(float crushPct, float downsamplePct) {
  const float a = std::max(crushPct, downsamplePct) * .01f;
  return a <= 0.f ? 0 : a < .3f ? 2 : a < .6f ? 4 : 5;
 }

 void prepare(double sampleRate) {
  sr = sampleRate > 0 ? sampleRate : 48000.0;
  cores[0].reset(); cores[1].reset();
  active = 0; fade = 1.f; current = -1;
  fadeStep = static_cast<float>(1.0 / (.03 * sr));
  satNow = sat;
 }
 // choice: 0 = off; saturation 0..1
 void set(int choice, float saturation) {
  sat = std::clamp(saturation, 0.f, 1.f);
  if (choice == current) return;
  // the new rate starts in the other core, faded in over 30 ms
  const int next = 1 - active;
  cores[static_cast<std::size_t>(next)].reset();
  cores[static_cast<std::size_t>(next)].design(rateOf(choice), sr);
  if (current < 0) { active = next; fade = 1.f; }
  else { active = next; fade = 0.f; }
  current = choice;
 }
 bool enabled() const { return current > 0 || fade < 1.f || satNow > 1e-4f || sat > 1e-4f; }

 void process(float& l, float& r) {
  float al = l, ar = r;
  cores[static_cast<std::size_t>(active)].run(al, ar);
  if (fade < 1.f) {
   float bl = l, br = r;
   cores[static_cast<std::size_t>(1 - active)].run(bl, br);
   al = bl + fade * (al - bl);
   ar = br + fade * (ar - br);
   fade = std::min(1.f, fade + fadeStep);
  }
  satNow += .001f * (sat - satNow);
  if (satNow > 1e-4f) { al = weight(al, satNow); ar = weight(ar, satNow); }
  l = al; r = ar;
 }

private:
 struct Biquad {
  double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, zl1 = 0, zl2 = 0, zr1 = 0, zr2 = 0;
  void reset() { zl1 = zl2 = zr1 = zr2 = 0; }
  void run(double& l, double& r) {
   const double yl = b0 * l + zl1; zl1 = b1 * l - a1 * yl + zl2; zl2 = b2 * l - a2 * yl; l = yl;
   const double yr = b0 * r + zr1; zr1 = b1 * r - a1 * yr + zr2; zr2 = b2 * r - a2 * yr; r = yr;
  }
 };
 // One rate: low pass, hold, low pass.
 struct Core {
  std::array<Biquad, 4> pre{}, post{};
  Biquad droop; // the hold dulls the top of the passband a little: put it back
  double step = 0.0, phase = 1.0, heldL = 0.0, heldR = 0.0, lastL = 0.0, lastR = 0.0;
  bool bypass = true;
  void reset() { for (auto& b : pre) b.reset(); for (auto& b : post) b.reset(); droop.reset(); phase = 1.0; heldL = heldR = lastL = lastR = 0.0; }
  void design(float rate, double sr) {
   bypass = rate <= 0.f || rate >= .98f * static_cast<float>(sr);
   if (bypass) return;
   step = static_cast<double>(rate) / sr;
   // 8-pole Butterworth just under the new Nyquist
   const double fc = std::min(.45 * static_cast<double>(rate), .45 * sr), pi = 3.14159265358979323846;
   const double w = std::tan(pi * fc / sr);
   static const double q[4] = {0.5097955791, 0.6013448869, 0.9000123967, 2.5629154477};
   for (int k = 0; k < 4; ++k) {
    const double n = 1.0 / (1.0 + w / q[k] + w * w);
    Biquad b;
    b.b0 = w * w * n; b.b1 = 2 * b.b0; b.b2 = b.b0;
    b.a1 = 2 * (w * w - 1) * n; b.a2 = (1 - w / q[k] + w * w) * n;
    pre[static_cast<std::size_t>(k)] = b; post[static_cast<std::size_t>(k)] = b;
   }
   { // RBJ peaking, +1.2 dB around a third of the new rate
    const double A = std::pow(10.0, 1.2 / 40.0), w0 = 2 * pi * std::min(.33 * static_cast<double>(rate), .4 * sr) / sr;
    const double al = std::sin(w0) / (2 * .7), cs = std::cos(w0), a0 = 1 + al / A;
    droop = Biquad{(1 + al * A) / a0, -2 * cs / a0, (1 - al * A) / a0, -2 * cs / a0, (1 - al / A) / a0};
   }
  }
  void run(float& l, float& r) {
   if (bypass) return;
   double x = l, y = r;
   for (auto& b : pre) b.run(x, y);
   // the new sample clock; each new sample is read at its exact instant
   phase += step;
   if (phase >= 1.0) {
    phase -= 1.0;
    const double t = 1.0 - phase / step; // where between the last input and this one the clock ticked
    heldL = lastL + t * (x - lastL);
    heldR = lastR + t * (y - lastR);
   }
   lastL = x; lastR = y;
   double ol = heldL, or_ = heldR;
   for (auto& b : post) b.run(ol, or_);
   droop.run(ol, or_);
   l = static_cast<float>(ol); r = static_cast<float>(or_);
  }
 };
 // Saturation for weight: a soft, slightly uneven curve, level kept.
 static float weight(float x, float amount) {
  const float drive = 1.f + 4.f * amount;
  const float d = x * drive + .08f * amount;
  const float y = d / (1.f + std::abs(d)) - (.08f * amount) / (1.f + .08f * amount);
  return y * (1.f + 1.f * amount) / drive; // about level at -10 dBFS; peaks rounded off, quiet detail lifted
 }

 double sr = 48000.0;
 std::array<Core, 2> cores{};
 int active = 0, current = -1;
 float fade = 1.f, fadeStep = .001f, sat = 0.f, satNow = 0.f;
};

} // namespace inverso_abismo
