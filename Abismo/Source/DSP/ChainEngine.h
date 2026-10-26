#pragma once
// One strip, whatever the plugin: input -> gate -> transients -> EQ -> tame -> compressor -> glue compressor
// -> saturation -> width -> limiter -> output (+ dither). A module that is off costs nothing and changes
// nothing (the limiter, when off, still delays so that switching it does not move the audio).
// Portable (no JUCE): the processor feeds it the parameter values once per block.
#include "ChainDyn.h"
#include "ChainParams.h"
#include <atomic>

namespace inverso_abismo::chain {

class Engine {
public:
 explicit Engine(Strip s = Strip::Master) : strip(s), values(layout(s).defaults), withLimiter(layout(s).local[P::LimOn] >= 0) {}
 int latency() const { return hasLimiter() ? limiter.latency() : 0; }
 bool hasLimiter() const { return withLimiter; }
 void prepare(double sampleRate) {
  sr = sampleRate;
  eq.prepare(sr); gate.prepare(sr); trans.prepare(sr); tame.prepare(sr); comp1.prepare(sr); comp2.prepare(sr);
  sat.prepare(sr); width.prepare(sr); limiter.prepare(sr); loud.prepare(sr);
  smoothCoef = timeCoef(.02, sr);
  setValues(values);
  inSmooth = inGain; outSmooth = outGain;
  reset();
 }
 void reset() {
  loud.reset();
  grComp1 = grComp2 = grLimiter = 0.f;
 }
 void setValues(const Values& v) {
  values = v;
  auto b = [&](int p) { return v[static_cast<std::size_t>(p)] >= .5f; };
  auto f = [&](int p) { return v[static_cast<std::size_t>(p)]; };
  auto i = [&](int p) { return static_cast<int>(std::lround(v[static_cast<std::size_t>(p)])); };
  const auto& l = layout(strip);
  auto has = [&](int p) { return l.local[static_cast<std::size_t>(p)] >= 0; };

  inGain = dbToGain(f(P::InGain));
  outGain = dbToGain(f(P::OutGain));

  // The master's macros sit on top of the preset and the detailed modules: the same parameters, pushed further.
  const float mGlue = has(P::MGlue) ? f(P::MGlue) * .01f : 0.f, mWarm = has(P::MWarmth) ? f(P::MWarmth) * .01f : 0.f,
              mTone = has(P::MTone) ? f(P::MTone) * .01f : 0.f, mLoud = has(P::MLoud) ? f(P::MLoud) * .01f : 0.f;

  EqControls e;
  e.on = b(P::EqOn); e.hpf = f(P::Hpf); e.lpf = f(P::Lpf); e.lsF = f(P::LsF); e.lsG = f(P::LsG);
  e.b1F = f(P::B1F); e.b1G = f(P::B1G); e.b1Q = f(P::B1Q); e.b2F = f(P::B2F); e.b2G = f(P::B2G); e.b2Q = f(P::B2Q);
  e.hsF = f(P::HsF); e.hsG = f(P::HsG); e.ms = i(P::EqMs);
  e.hsG += 3.5f * mTone; e.lsG += 1.5f * mWarm - 1.2f * mTone;   // TONE: bright to dark; WARMTH adds weight
  if (mTone > 1e-4f || mTone < -1e-4f || mWarm > 0.f) e.on = true;
  eq.setControls(e);

  GateControls g;
  g.on = has(P::GateOn) && b(P::GateOn); g.thresh = f(P::GateThresh); g.range = f(P::GateRange);
  g.holdMs = f(P::GateHold); g.releaseMs = f(P::GateRelease); g.focus = f(P::GateFocus); g.focusOn = f(P::GateFocus) > 20.5f;   // as the knob reads it: 20 Hz is OFF
  gate.setControls(g);

  TransControls t;
  t.on = has(P::TrOn) && b(P::TrOn); t.attack = f(P::TrAttack); t.sustain = f(P::TrSustain);
  trans.setControls(t);

  TameControls m;
  m.on = has(P::TameOn) && b(P::TameOn); m.freq = f(P::TameFreq); m.thresh = f(P::TameThresh); m.range = f(P::TameRange); m.q = f(P::TameQ);
  tame.setControls(m);

  CompControls c;
  c.on = b(P::C1On); c.type = static_cast<CompType>(juce_clamp(i(P::C1Type), 0, 3)); c.thresh = f(P::C1Thresh); c.ratio = f(P::C1Ratio);
  c.attackMs = f(P::C1Attack); c.releaseMs = f(P::C1Release); c.knee = f(P::C1Knee); c.timeIdx = juce_clamp(i(P::C1Time) - 1, 0, 5);
  c.mix = f(P::C1Mix); c.makeup = f(P::C1Makeup); c.scHpf = f(P::C1Sc); c.autoMakeup = true;
  comp1.setControls(c);

  CompControls k;
  k.on = has(P::C2On) && b(P::C2On); k.type = static_cast<CompType>(juce_clamp(i(P::C2Type), 0, 3)); k.thresh = f(P::C2Thresh); k.ratio = f(P::C2Ratio);
  k.attackMs = f(P::C2Attack); k.releaseMs = f(P::C2Release); k.knee = 6.f; k.timeIdx = 2; k.mix = f(P::C2Mix); k.makeup = f(P::C2Makeup);
  k.scHpf = 90.f; k.autoMakeup = true;
  if (mGlue > 0.f) {   // GLUE: the glue compressor, further and further in
   if (!k.on) { k.on = true; k.type = CompType::Vca; k.thresh = -6.f; k.ratio = 1.5f; k.attackMs = 30.f; k.releaseMs = 250.f; }
   k.thresh -= 14.f * mGlue; k.ratio = std::max(k.ratio, 1.5f + 2.5f * mGlue);
  }
  comp2.setControls(k);

  SatControls s;
  s.on = b(P::SatOn) || mWarm > 0.f; s.type = static_cast<SatType>(juce_clamp(i(P::SatKind), 0, 2)); s.drive = std::min(100.f, f(P::SatDrive) + 60.f * mWarm); s.tone = std::max(-100.f, f(P::SatTone) - 20.f * mWarm); s.mix = f(P::SatMix);
  sat.setControls(s);

  WidthControls w;
  w.width = has(P::StereoWidth) ? f(P::StereoWidth) : 100.f; w.bassMono = has(P::BassMono) ? f(P::BassMono) : 20.f;
  width.setControls(w);

  LimiterControls q;
  q.on = has(P::LimOn) && (b(P::LimOn) || mLoud > 0.f); q.gainDb = f(P::LimGain) + 12.f * mLoud; q.ceilingDb = f(P::LimCeil); q.releaseMs = f(P::LimRelease); q.autoRelease = b(P::LimAuto);
  limiter.setControls(q);
 
  const int bits = i(P::DitherBits);
  dither[0].setBits(bits == 1 ? 16 : bits == 2 ? 20 : bits == 3 ? 24 : 0);
  dither[1].setBits(bits == 1 ? 16 : bits == 2 ? 20 : bits == 3 ? 24 : 0);
 }

 // right may be null (a mono track): it is processed as two identical channels and the left comes out.
 void process(float* left, float* right, int n) {
  if (resetRequest.exchange(false, std::memory_order_relaxed)) loud.reset();
  for (int s = 0; s < n; ++s) {
   inSmooth += (inGain - inSmooth) * smoothCoef;
   outSmooth += (outGain - outSmooth) * smoothCoef;
   double l = left[s] * inSmooth, r = (right ? right[s] : left[s]) * inSmooth;
   gate.process(l, r);
   trans.process(l, r);
   eq.process(l, r);
   tame.process(l, r);
   comp1.process(l, r);
   comp2.process(l, r);
   l = sat.process(0, l); r = sat.process(1, r);
   width.process(l, r);
   if (withLimiter) limiter.process(l, r);
   l *= outSmooth; r *= outSmooth;
   loud.process(l, r);
   loud.peak(l, r);
   l = dither[0].process(l); r = dither[1].process(r);
   left[s] = static_cast<float>(l);
   if (right) right[s] = static_cast<float>(r);
  }
  grComp1 = static_cast<float>(comp1.grDb());
  grComp2 = static_cast<float>(comp2.grDb());
  grLimiter = static_cast<float>(limiter.grDb());
  grTame = static_cast<float>(tame.grDb());
  gateOpen = gate.opening() > .5 ? 1.f : 0.f;
  momentary.store(static_cast<float>(loud.momentaryLufs()), std::memory_order_relaxed);
  shortTerm.store(static_cast<float>(loud.shortTermLufs()), std::memory_order_relaxed);
  integrated.store(static_cast<float>(loud.integratedLufs()), std::memory_order_relaxed);
  peakDb.store(static_cast<float>(loud.peakDb()), std::memory_order_relaxed);
  gr1.store(grComp1, std::memory_order_relaxed);
  gr2.store(grComp2, std::memory_order_relaxed);
  grLim.store(grLimiter, std::memory_order_relaxed);
  grTm.store(grTame, std::memory_order_relaxed);
  gateNow.store(gateOpen, std::memory_order_relaxed);
 }

 void resetMeters() {
  resetRequest.store(true, std::memory_order_relaxed);
  momentary.store(-70.f); shortTerm.store(-70.f); integrated.store(-70.f); peakDb.store(-120.f);   // at once, even with the host stopped
 } // the editor asks; the audio thread does it

 Strip strip;
 // read by the editor
 std::atomic<float> momentary{-70.f}, shortTerm{-70.f}, integrated{-70.f}, peakDb{-120.f};
 std::atomic<float> gr1{0.f}, gr2{0.f}, grLim{0.f}, grTm{0.f}, gateNow{0.f};
 std::atomic<bool> resetRequest{false};

private:
 static int juce_clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
 double sr = 48000.0;
 Values values;
 Eq eq; Gate gate; Transient trans; Tame tame; Compressor comp1, comp2; Saturator sat; Width width; Limiter limiter; Loudness loud;
 std::array<Dither, 2> dither;
 double inGain = 1.0, outGain = 1.0, inSmooth = 1.0, outSmooth = 1.0, smoothCoef = .01;
 float grComp1 = 0.f, grComp2 = 0.f, grLimiter = 0.f, grTame = 0.f, gateOpen = 0.f;
 const bool withLimiter;
};

} // namespace inverso_abismo::chain
