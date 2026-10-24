#pragma once
// HALO engine: pre-delay, a low cut and a high cut on what goes into the
// reverb, the shared FDN reverb voiced as a plate, a hall, a room or the gated
// room, and for the gated room a gate keyed by the dry signal: each hit opens
// it, it stays open for HOLD after the sound drops under the threshold, then
// closes in RELEASE, so the big space stops dead. No latency. Portable (no JUCE).
#include "AbismoCommon.h"
#include "AbismoReverb.h"
#include "HaloParams.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

namespace inverso_abismo::halo {

class Engine {
public:
 void prepare(double sampleRate) {
  sr = sampleRate > 0 ? sampleRate : 48000.0;
  const auto n = static_cast<std::size_t>(.3 * sr) + 8;
  preL.assign(n, 0.f); preR.assign(n, 0.f); write = 0;
  reverb.prepare(sr);
  hpL = hpR = lp1L = lp1R = lp2L = lp2R = 0.f;
  keyEnv = 0.f; gate = 0.f; holdLeft = 0;
  smoothed = target;
  control();
  mixNow = mix; outNow = out; preNow = preSamples;
 }
 void setValues(const Values& v) { target = v; }
 void reset() {
  std::fill(preL.begin(), preL.end(), 0.f); std::fill(preR.begin(), preR.end(), 0.f);
  reverb.clear(); hpL = hpR = lp1L = lp1R = lp2L = lp2R = 0.f; keyEnv = 0.f; gate = 0.f; holdLeft = 0;
 }

 void process(float* left, float* right, int n) {
  const bool stereo = right != nullptr && right != left;
  int done = 0;
  while (done < n) {
   if (countdown <= 0) { control(); countdown = 32; }
   const int m = std::min(countdown, n - done);
   for (int i = done; i < done + m; ++i) {
    const float dl = left[i], dr = stereo ? right[i] : left[i];
    // pre-delay
    preL[write] = dl; preR[write] = dr;
    // the read point glides (a pre-delay turned while playing does not click) and is interpolated
    preNow += std::clamp(.0008f * (preSamples - preNow), -.25f, .25f); // at most a quarter-tone-ish slide, never a jump
    const std::size_t size = preL.size();
    const float back = std::clamp(preNow, 0.f, static_cast<float>(size - 3));
    const auto whole = static_cast<std::size_t>(back);
    const float frac = back - static_cast<float>(whole);
    const std::size_t a0 = (write + size - whole) % size, a1 = (a0 + size - 1) % size;
    float sl = preL[a0] + frac * (preL[a1] - preL[a0]), sr2 = preR[a0] + frac * (preR[a1] - preR[a0]);
    if (++write >= size) write = 0;
    // what goes in: no mud below LOW CUT, no fizz above HIGH CUT
    hpL += hpCoef * (sl - hpL); hpR += hpCoef * (sr2 - hpR);
    sl -= hpL; sr2 -= hpR;
    lp1L += lpCoef * (sl - lp1L); lp2L += lpCoef * (lp1L - lp2L);
    lp1R += lpCoef * (sr2 - lp1R); lp2R += lpCoef * (lp1R - lp2R);
    float wl, wr;
    reverb.process(lp2L, lp2R, wl, wr);
    // the gate, keyed by the dry sound
    if (gated) {
     const float key = std::max(std::abs(dl), std::abs(dr));
     keyEnv = key > keyEnv ? key : keyEnv * keyFall;
     if (keyEnv > threshold) { holdLeft = holdSamples; gateTarget = 1.f; }
     else if (holdLeft > 0) --holdLeft;
     else gateTarget = 0.f;
     gate += (gateTarget - gate) * (gateTarget > gate ? gateOpen : gateClose);
     wl *= gate; wr *= gate;
    } else gate += (1.f - gate) * .01f;
    // width of the reverb, then the mix
    const float mid = .5f * (wl + wr), side = .5f * (wl - wr) * width;
    wl = mid + side; wr = mid - side;
    mixNow += .002f * (mix - mixNow);
    outNow += .002f * (out - outNow);
    const float dry = std::cos(mixNow * 1.5707963f), wet = std::sin(mixNow * 1.5707963f) * 1.4f;
    left[i] = (dl * dry + wl * wet) * outNow;
    if (stereo) right[i] = (dr * dry + wr * wet) * outNow;
   }
   done += m; countdown -= m;
  }
  gateNow.store(gated ? gate : -1.f, std::memory_order_relaxed);
  keyNow.store(20.f * std::log10(keyEnv + 1e-9f), std::memory_order_relaxed);
 }

 std::atomic<float> gateNow{0.f}, keyNow{-120.f}; // for the editor

private:
 void control() {
  for (std::size_t i = 0; i < smoothed.size(); ++i) {
   if (specs()[i].kind == Kind::Float) smoothed[i] += .3f * (target[i] - smoothed[i]);
   else smoothed[i] = target[i];
  }
  const auto& s = smoothed;
  const auto srf = static_cast<float>(sr);
  const int mode = static_cast<int>(std::lround(s[P::Mode]));
  const float sz = s[P::Size] * .01f, damp = s[P::Damp] * .01f;
  float size = sz, decay = s[P::Decay], damping = damp;
  switch (mode) {
   case 0: size = .3f + .35f * sz; damping = damp * .7f; break;           // plate: dense, bright, quick to build
   case 1: size = .55f + .45f * sz; break;                                // hall
   case 2: size = .1f + .4f * sz; decay = std::min(decay, 3.f); break;    // room
   default: size = .6f + .4f * sz; decay = std::max(decay, 2.f); break;   // gated: a big room, cut by the gate
  }
  reverb.configure(size, decay, damping, false);
  gated = mode == 3;
  preSamples = std::clamp(s[P::PreDelay] * .001f * srf, 0.f, static_cast<float>(preL.size() - 2));
  hpCoef = 1.f - std::exp(-6.2831853f * s[P::LowCut] / srf);
  lpCoef = 1.f - std::exp(-6.2831853f * std::min(s[P::HighCut], .45f * srf) / srf);
  threshold = std::pow(10.f, s[P::GateThresh] / 20.f);
  holdSamples = static_cast<int>(s[P::GateHold] * .001f * srf);
  gateOpen = 1.f - std::exp(-1.f / (.001f * srf));
  gateClose = 1.f - std::exp(-1.f / (s[P::GateRelease] * .001f * srf * .3f));
  keyFall = std::exp(-1.f / (.03f * srf));
  width = s[P::Width] * .01f;
  mix = s[P::Mix] * .01f;
  out = std::pow(10.f, s[P::Output] / 20.f);
 }

 double sr = 48000.0;
 std::vector<float> preL, preR;
 std::size_t write = 0;
 Reverb reverb;
 Values target = defaults(), smoothed = defaults();
 int countdown = 0, holdSamples = 0, holdLeft = 0;
 bool gated = true;
 float preSamples = 0.f, preNow = 0.f, hpCoef = .01f, lpCoef = .5f, threshold = .03f, gateOpen = .02f, gateClose = .01f, keyFall = .999f;
 float hpL = 0.f, hpR = 0.f, lp1L = 0.f, lp1R = 0.f, lp2L = 0.f, lp2R = 0.f, keyEnv = 0.f, gate = 0.f, gateTarget = 0.f;
 float width = 1.f, mix = .3f, out = 1.f, mixNow = .3f, outNow = 1.f;
};

} // namespace inverso_abismo::halo
