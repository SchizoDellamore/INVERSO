#pragma once
// INVERSO channel-strip and mastering dynamics (no JUCE): the compressor in four
// characters, the gate, the transient shaper, the dynamic band (de-esser /
// tamer), the true-peak limiter, the BS.1770 loudness meter and the dither.
#include "ChainDsp.h"

namespace inverso_abismo::chain {

// ---------------------------------------------------------------- compressor

enum class CompType { VarMu = 0, Vca = 1, Fet = 2, Opto = 3 };

struct CompControls {
 bool on = true;
 CompType type = CompType::Vca;
 float thresh = -18.f;     // dB
 float ratio = 4.f;        // Vca / Fet / Opto
 float attackMs = 10.f;
 float releaseMs = 100.f;  // 0 = auto (two time constants that follow the program)
 float knee = 4.f;         // dB
 int timeIdx = 0;          // VarMu: the six time constants, 0..5
 float mix = 100.f;        // % wet (parallel compression below 100)
 float scHpf = 60.f;       // Hz, the detector does not hear below it
 float makeup = 0.f;       // dB
 bool autoMakeup = true;
 float link = 100.f;       // % stereo link
 bool ms = false;          // detect and act on mid and side separately
};

class Compressor {
public:
 void prepare(double sampleRate) {
  sr = sampleRate;
  for (auto& ch : s) ch = Channel{};
  setControls(c);
 }
 void setControls(const CompControls& n) {
  c = n;
  scCoeffs = highpass(clampd(c.scHpf, 20.0, 500.0), .7071, sr);
  for (auto& ch : s) ch.sc.set(scCoeffs);
  linkAmt = clampd(c.link, 0, 100) / 100.0;
  mixAmt = clampd(c.mix, 0, 100) / 100.0;
  // time constants
  double a = std::max(c.attackMs, .01f) * .001, r1 = std::max(c.releaseMs, 1.f) * .001, r2 = 0.0, w2 = 0.0;
  switch (c.type) {
   case CompType::VarMu: {
    static const double att[6] = {.0002, .0002, .0002, .0004, .0002, .0008};
    static const double rel1[6] = {.3, .8, 2.0, 5.0, 2.0, .3};
    static const double rel2[6] = {0, 0, 0, 0, 10.0, 25.0};
    const int t = std::clamp(c.timeIdx, 0, 5);
    a = att[t]; r1 = rel1[t]; r2 = rel2[t]; w2 = r2 > 0 ? .5 : 0.0;
    break;
   }
   case CompType::Vca:
    if (c.releaseMs <= 0.5f) { r1 = .05; r2 = .5; w2 = .5; }
    break;
   case CompType::Fet:
    a = std::max(a, .00002);
    if (c.releaseMs <= 0.5f) { r1 = .06; r2 = .4; w2 = .4; }
    break;
   case CompType::Opto:
    a = std::max(a, .008);
    if (c.releaseMs <= 0.5f) { r1 = .06; r2 = 1.2; w2 = .6; } else { r2 = r1; r1 = std::min(.06, r1 * .2); w2 = .6; }
    break;
  }
  attCoef = timeCoef(a, sr); rel1Coef = timeCoef(r1, sr); rel2Coef = r2 > 0 ? timeCoef(r2, sr) : 0.0; weight2 = w2;
  rmsCoef = timeCoef(c.type == CompType::Fet ? .0005 : .005, sr);
  peakShare = c.type == CompType::Fet ? 1.0 : (c.type == CompType::Vca ? .5 : .2);
  peakDecay = std::exp(-1.0 / (.005 * sr));
  // make-up: the loudness lost by the average gain reduction on typical program
  const double ratioEff = c.type == CompType::VarMu ? 3.0 : std::max(1.2f, c.ratio);
  makeupDb = (c.autoMakeup ? -.5 * c.thresh * (1.0 - 1.0 / ratioEff) : 0.0) + c.makeup;
  if (c.type == CompType::VarMu && c.autoMakeup) makeupDb = std::max(0.0, makeupDb) * .8 + c.makeup;
 }
 // Static gain reduction (dB, positive) for an input level.
 double staticGr(double levelDb) const {
  const double over = levelDb - c.thresh;
  double o;
  const double k = c.type == CompType::VarMu ? std::max(c.knee, 6.f) : std::max(c.knee, 0.f);
  if (k > .01) { if (over < -k / 2) o = 0; else if (over > k / 2) o = over; else o = (over + k / 2) * (over + k / 2) / (2 * k); }
  else o = std::max(over, 0.0);
  if (o <= 0) return 0.0;
  if (c.type == CompType::VarMu) { const double a = .11; return o - std::log(1.0 + a * o) / a; }
  return o * (1.0 - 1.0 / std::max(1.0f, c.ratio));
 }
 void process(double& l, double& r) {
  if (!c.on) { meter = 0.0; return; }
  double in[2] = {l, r};
  if (c.ms) { in[0] = .5 * (l + r); in[1] = .5 * (l - r); }
  double lvl[2];
  for (int i = 0; i < 2; ++i) {
   auto& ch = s[static_cast<std::size_t>(i)];
   const double x = ch.sc.process(in[i]);
   const double a = std::abs(x);
   ch.peak = std::max(a, ch.peak * peakDecay);
   ch.ms += rmsCoef * (x * x - ch.ms);
   lvl[i] = peakShare * ch.peak + (1.0 - peakShare) * std::sqrt(std::max(ch.ms, 0.0)) * 1.4142;
  }
  const double top = std::max(lvl[0], lvl[1]);
  double out[2];
  double worst = 0.0;
  for (int i = 0; i < 2; ++i) {
   auto& ch = s[static_cast<std::size_t>(i)];
   const double linked = lvl[i] + linkAmt * (top - lvl[i]);
   const double target = staticGr(gainToDb(linked));
   // two followers: the second is the slow release, used by the auto settings
   ch.a += (target > ch.a ? attCoef : rel1Coef) * (target - ch.a);
   if (weight2 > 0) ch.b += (target > ch.b ? attCoef : rel2Coef) * (target - ch.b); else ch.b = ch.a;
   const double gr = ch.a + weight2 * (ch.b - ch.a);
   worst = std::max(worst, gr);
   const double wet = in[i] * dbToGain(makeupDb - gr);
   out[i] = in[i] + (wet - in[i]) * mixAmt;
  }
  meter = worst;
  if (c.ms) { l = out[0] + out[1]; r = out[0] - out[1]; } else { l = out[0]; r = out[1]; }
 }
 double grDb() const { return meter; }
private:
 struct Channel { Biquad sc; double peak = 0, ms = 0, a = 0, b = 0; };
 double sr = 48000.0, linkAmt = 1.0, mixAmt = 1.0, attCoef = 1.0, rel1Coef = .001, rel2Coef = .0001, weight2 = 0.0, rmsCoef = .01, peakShare = .5, peakDecay = .9, makeupDb = 0.0, meter = 0.0;
 CompControls c;
 Coeffs scCoeffs;
 std::array<Channel, 2> s;
};

// ---------------------------------------------------------------- gate

struct GateControls {
 bool on = false;
 float thresh = -40.f;   // dB
 float range = 30.f;     // dB of reduction when shut
 float attackMs = .5f, holdMs = 40.f, releaseMs = 120.f;
 float focus = 200.f;    // Hz: what the gate listens to
 bool focusOn = false;
};

class Gate {
public:
 void prepare(double sampleRate) { sr = sampleRate; env = 0; gain = 1; holdLeft = 0; open = false; for (auto& b : bp) b.reset(); setControls(c); }
 void setControls(const GateControls& n) {
  c = n;
  if (c.focusOn) for (auto& b : bp) b.set(bandpass(c.focus, 1.1, sr));
  floorGain = dbToGain(-clampd(c.range, 0, 90));
  attCoef = timeCoef(std::max(c.attackMs, .02f) * .001, sr);
  relCoef = timeCoef(std::max(c.releaseMs, 1.f) * .001, sr);
  holdSamples = static_cast<int>(c.holdMs * .001 * sr);
  envDecay = std::exp(-1.0 / (.004 * sr));
  threshLin = dbToGain(c.thresh);
 }
 void process(double& l, double& r) {
  if (!c.on) { openNow = 1.0; return; }
  double a = std::abs(l), b = std::abs(r);
  if (c.focusOn) { a = std::abs(bp[0].process(l)); b = std::abs(bp[1].process(r)); }
  env = std::max(std::max(a, b), env * envDecay);
  if (env > threshLin) { open = true; holdLeft = holdSamples; }
  else if (open && env < threshLin * .5) { if (holdLeft > 0) --holdLeft; else open = false; }  // 6 dB of hysteresis
  const double target = open ? 1.0 : floorGain;
  gain += (target > gain ? attCoef : relCoef) * (target - gain);
  l *= gain; r *= gain;
  openNow = gain;
 }
 double opening() const { return openNow; }
private:
 double sr = 48000.0, env = 0, gain = 1, floorGain = .03, attCoef = .1, relCoef = .001, envDecay = .99, threshLin = .01, openNow = 1.0;
 int holdSamples = 0, holdLeft = 0;
 bool open = false;
 GateControls c;
 std::array<Biquad, 2> bp;
};

// ---------------------------------------------------------------- transient shaper

struct TransControls {
 bool on = false;
 float attack = 0.f;    // -100..100
 float sustain = 0.f;   // -100..100
};

// The difference between a fast and a slow envelope: where the fast one is ahead the
// sound is attacking, where it is behind it is decaying. Independent of the level.
class Transient {
public:
 void prepare(double sampleRate) { sr = sampleRate; fast = slow = 1e-6; gain = 1.0; fa = timeCoef(.001, sr); fr = timeCoef(.03, sr); sa = timeCoef(.03, sr); sr2 = timeCoef(.25, sr); sm = timeCoef(.0006, sr); }
 void setControls(const TransControls& n) { c = n; }
 void process(double& l, double& r) {
  if (!c.on) return;
  const double x = std::max(std::abs(l), std::abs(r));
  fast += (x > fast ? fa : fr) * (x - fast);
  slow += (x > slow ? sa : sr2) * (x - slow);
  const double ev = clampd(std::log(std::max(fast, 1e-6) / std::max(slow, 1e-6)), -1.5, 1.5);
  // fast ahead of slow: an onset (the ATTACK knob, up to 12 dB); behind it: a tail (the SUSTAIN knob)
  const double target = dbToGain(ev > 0 ? c.attack * .12 * (ev / 1.5) : c.sustain * .12 * (-ev / 1.5));
  gain += sm * (target - gain);
  l *= gain; r *= gain;
 }
private:
 double sr = 48000.0, fast = 1e-6, slow = 1e-6, gain = 1.0, fa = .1, fr = .01, sa = .01, sr2 = .001, sm = .1;
 TransControls c;
};

// ---------------------------------------------------------------- dynamic band (de-esser / tamer)

struct TameControls {
 bool on = false;
 float freq = 6500.f;
 float thresh = -24.f;   // dB
 float range = 8.f;      // dB at most
 float q = 2.f;
};

class Tame {
public:
 void prepare(double sampleRate) { sr = sampleRate; env = 0; gr = 0; counter = 0; for (auto& b : bp) b.reset(); for (auto& b : cut) b.reset(); setControls(c); }
 void setControls(const TameControls& n) {
  c = n;
  for (auto& b : bp) b.set(bandpass(c.freq, c.q, sr));
  att = timeCoef(.0004, sr); rel = timeCoef(.04, sr); smooth = timeCoef(.002, sr);
 }
 void process(double& l, double& r) {
  if (!c.on) { meter = 0.0; return; }
  const double d = std::max(std::abs(bp[0].process(l)), std::abs(bp[1].process(r)));
  env += (d > env ? att : rel) * (d - env);
  const double over = gainToDb(env) - c.thresh;
  const double target = clampd(over * .75, 0.0, c.range);   // 4:1 above the threshold, never more than RANGE
  gr += smooth * (target - gr);
  if (++counter >= 16 && std::abs(gr - applied) > .02) {
   counter = 0; applied = gr;
   const Coeffs k = peaking(c.freq, c.q, -gr, sr);
   for (auto& b : cut) b.set(k);
  } else if (counter >= 16) counter = 0;
  if (applied > .02) { l = cut[0].process(l); r = cut[1].process(r); } else { cut[0].process(l); cut[1].process(r); }
  meter = applied;
 }
 double grDb() const { return meter; }
private:
 double sr = 48000.0, env = 0, gr = 0, applied = 0, att = .1, rel = .001, smooth = .1, meter = 0.0;
 int counter = 0;
 TameControls c;
 std::array<Biquad, 2> bp, cut;
};

// ---------------------------------------------------------------- true-peak limiter

struct LimiterControls {
 bool on = true;
 float gainDb = 0.f;      // push into the limiter
 float ceilingDb = -1.f;  // dBTP
 float releaseMs = 200.f;
 bool autoRelease = true;
};

// Looks ahead of the signal and brings the level down smoothly before each peak, measured as the
// peak of the reconstructed (4x) waveform so the output does not go over the ceiling between samples.
class Limiter {
public:
 static constexpr int taps = 12;        // per side, per interpolation phase
 static constexpr int lookahead = 96;   // samples of smoothing (about 2 ms)
 int latency() const { return taps + look - 1; }   // follows the sample rate: valid after prepare()
 void prepare(double sampleRate) {
  sr = sampleRate;
  const double ratio = sampleRate / 48000.0;
  look = std::max(24, static_cast<int>(std::lround(lookahead * ratio)));
  ring = 1; while (ring < 2 * taps + look + 8) ring <<= 1;
  for (auto& b : x) b.assign(static_cast<std::size_t>(ring), 0.0);
  req.assign(static_cast<std::size_t>(ring), 1.0);
  rbuf.assign(static_cast<std::size_t>(ring), 1.0);
  for (int p = 1; p < 4; ++p)
   for (int k = 0; k < 2 * taps; ++k) {
    const double t = static_cast<double>(k - taps + 1) - static_cast<double>(p) / 4.0;   // tap position relative to the interpolated point
    const double win = .42 + .5 * std::cos(pi * t / (taps + .5)) + .08 * std::cos(2 * pi * t / (taps + .5));
    const double sinc = std::abs(t) < 1e-9 ? 1.0 : std::sin(pi * t) / (pi * t);
    phase[static_cast<std::size_t>(p)][static_cast<std::size_t>(k)] = sinc * (std::abs(t) < taps + .5 ? win : 0.0);
   }
  pos = 0; sumR = 0; f1 = f2 = 1.0; density = 0; grMeter = 0;
  dq.assign(static_cast<std::size_t>(ring), 0); dqHead = dqTail = 0;
  setControls(c);
 }
 void setControls(const LimiterControls& n) {
  c = n;
  push = dbToGain(c.gainDb);
  ceil = dbToGain(c.ceilingDb);
  c1 = timeCoef(c.autoRelease ? .02 : c.releaseMs * .001, sr);
  c2 = timeCoef(c.autoRelease ? .4 : c.releaseMs * .001, sr);
 }
 // One stereo sample in, one out (delayed by latency()).
 void process(double& l, double& r) {
  if (!c.on) { delayLine(l, r); return; }
  const std::size_t mask = static_cast<std::size_t>(ring - 1);
  x[0][pos & mask] = l * push; x[1][pos & mask] = r * push;
  // the detection index j: the newest sample is pos, the newest with `taps` future samples is pos - taps
  const long j = static_cast<long>(pos) - taps;
  double peak = 0.0;
  if (j >= 0) {
   for (std::size_t ch = 0; ch < 2; ++ch) {
    const auto& b = x[ch];
    peak = std::max(peak, std::abs(b[static_cast<std::size_t>(j) & mask]));
    for (int p = 1; p < 4; ++p) {
     double acc = 0.0;
     for (int k = 0; k < 2 * taps; ++k) acc += phase[static_cast<std::size_t>(p)][static_cast<std::size_t>(k)] * b[static_cast<std::size_t>(j + k - taps + 1) & mask];
     peak = std::max(peak, std::abs(acc));
    }
   }
  }
  const double g = peak > ceil ? ceil / peak : 1.0;
  req[pos & mask] = g;
  // sliding minimum of the requirement over `look` samples
  while (dqHead != dqTail && req[dq[(dqTail - 1) & mask] & mask] >= g) --dqTail;
  dq[dqTail & mask] = pos; ++dqTail;
  while (dq[dqHead & mask] + static_cast<std::size_t>(look) <= pos) ++dqHead;
  const double m = req[dq[dqHead & mask] & mask];
  // release: it goes down at once, up slowly; two speeds blended by how busy the limiting is
  f1 = std::min(m, f1 + (1.0 - f1) * c1);
  f2 = std::min(m, f2 + (1.0 - f2) * c2);
  density += .0002 * ((1.0 - m) * 12.0 - density);
  const double w = c.autoRelease ? clampd(density, 0.0, 1.0) : 1.0;
  const double rr = f1 + w * (f2 - f1);
  // average over `look` samples: the gain reaches its value by the time the peak arrives
  rbuf[pos & mask] = rr; sumR += rr;
  if (pos >= static_cast<std::size_t>(look)) sumR -= rbuf[(pos - static_cast<std::size_t>(look)) & mask];
  const double count = static_cast<double>(std::min<std::size_t>(pos + 1, static_cast<std::size_t>(look)));
  const double gs = pos < static_cast<std::size_t>(look) ? 1.0 : sumR / count;
  // the sample that leaves now is the one the gain was prepared for: look - 1 samples back from j
  const long out = static_cast<long>(pos) - (taps + look - 1);
  double ol = 0.0, orr = 0.0;
  if (out >= 0) { ol = x[0][static_cast<std::size_t>(out) & mask] * gs; orr = x[1][static_cast<std::size_t>(out) & mask] * gs; }
  // safety: never beyond the ceiling in sample value either
  l = clampd(ol, -ceil, ceil); r = clampd(orr, -ceil, ceil);
  grMeter = -gainToDb(gs);
  ++pos;
 }
 double grDb() const { return grMeter; }
private:
 void delayLine(double& l, double& r) {   // limiter off: only the latency, so switching it does not shift the audio
  const std::size_t mask = static_cast<std::size_t>(ring - 1);
  x[0][pos & mask] = l; x[1][pos & mask] = r;
  const long out = static_cast<long>(pos) - (taps + look - 1);
  l = out >= 0 ? x[0][static_cast<std::size_t>(out) & mask] : 0.0;
  r = out >= 0 ? x[1][static_cast<std::size_t>(out) & mask] : 0.0;
  grMeter = 0.0;
  ++pos;
 }
 double sr = 48000.0, push = 1.0, ceil = .89, c1 = .01, c2 = .001, f1 = 1.0, f2 = 1.0, density = 0.0, sumR = 0.0, grMeter = 0.0;
 int ring = 256, look = lookahead;
 std::size_t pos = 0, dqHead = 0, dqTail = 0;
 std::array<std::vector<double>, 2> x;
 std::vector<double> req, rbuf;
 std::vector<std::size_t> dq;
 std::array<std::array<double, 2 * taps>, 4> phase{};
 LimiterControls c;
};

// ---------------------------------------------------------------- loudness (ITU-R BS.1770-4 / EBU R128)

class Loudness {
public:
 void prepare(double sampleRate) {
  sr = sampleRate;
  const double f1 = 1681.974450955533, G = 3.999843853973347, Q1 = .7071752369554196;
  double K = std::tan(pi * f1 / sr);
  const double Vh = std::pow(10.0, G / 20.0), Vb = std::pow(Vh, .4996667741545416), a0 = 1.0 + K / Q1 + K * K;
  shelf = {(Vh + Vb * K / Q1 + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q1 + K * K) / a0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q1 + K * K) / a0};
  const double f2 = 38.13547087602444, Q2 = .5003270373238773;
  K = std::tan(pi * f2 / sr);
  const double b0 = 1.0 + K / Q2 + K * K;
  rlb = {1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / b0, (1.0 - K / Q2 + K * K) / b0};
  reset();
 }
 void reset() {
  for (auto& b : st1) b.reset();
  for (auto& b : st2) b.reset();
  for (auto& b : st1) b.set(shelf);
  for (auto& b : st2) b.set(rlb);
  blockSum = 0; blockCount = 0; blockLen = static_cast<int>(sr * .1);
  history.fill(0.0); head = 0; filled = 0;
  hist.fill(0); histEnergy.fill(0.0);
  momentary = shortTerm = integrated = -120.0;
  truePeakDb = -120.0;
 }
 void process(double l, double r) {
  const double a = st2[0].process(st1[0].process(l)), b = st2[1].process(st1[1].process(r));
  blockSum += a * a + b * b;
  if (++blockCount >= blockLen) {
   const double z = blockSum / static_cast<double>(blockCount);
   blockSum = 0; blockCount = 0;
   history[head] = z; head = (head + 1) % history.size(); filled = std::min<std::size_t>(filled + 1, history.size());
   auto lufs = [](double e) { return e > 0 ? -.691 + 10.0 * std::log10(e) : -120.0; };
   if (filled >= 4) {
    double e = 0; for (std::size_t k = 0; k < 4; ++k) e += history[(head + history.size() - 1 - k) % history.size()];
    e /= 4.0; momentary = lufs(e);
    // gated blocks of 400 ms, every 100 ms
    if (momentary > -70.0) {
     const int bin = static_cast<int>(clampd((momentary + 70.0) * 10.0, 0.0, 799.0));
     ++hist[static_cast<std::size_t>(bin)]; histEnergy[static_cast<std::size_t>(bin)] += e;
    }
   }
   if (filled >= 30) { double e = 0; for (std::size_t k = 0; k < 30; ++k) e += history[(head + history.size() - 1 - k) % history.size()]; shortTerm = lufs(e / 30.0); }
   else if (filled >= 4) shortTerm = momentary;
   // integrated: absolute gate (-70) is the histogram's floor; the relative gate is 10 LU under the mean of what passed it
   double total = 0; long n = 0;
   for (std::size_t k = 0; k < hist.size(); ++k) { total += histEnergy[k]; n += hist[k]; }
   if (n > 0) {
    const double rel = lufs(total / static_cast<double>(n)) - 10.0;
    double e2 = 0; long n2 = 0;
    for (std::size_t k = 0; k < hist.size(); ++k) if (static_cast<double>(k) / 10.0 - 70.0 >= rel) { e2 += histEnergy[k]; n2 += hist[k]; }
    integrated = n2 > 0 ? lufs(e2 / static_cast<double>(n2)) : -120.0;
   }
  }
 }
 void peak(double l, double r) { truePeakDb = std::max(truePeakDb, gainToDb(std::max(std::abs(l), std::abs(r)))); }
 double momentaryLufs() const { return momentary; }
 double shortTermLufs() const { return shortTerm; }
 double integratedLufs() const { return integrated; }
 double peakDb() const { return truePeakDb; }
private:
 double sr = 48000.0, blockSum = 0.0, momentary = -120, shortTerm = -120, integrated = -120, truePeakDb = -120;
 int blockCount = 0, blockLen = 4800;
 Coeffs shelf, rlb;
 std::array<Biquad, 2> st1, st2;
 std::array<double, 64> history{};
 std::size_t head = 0, filled = 0;
 std::array<long, 800> hist{};
 std::array<double, 800> histEnergy{};
};

// ---------------------------------------------------------------- dither

class Dither {
public:
 void setBits(int bits) { lsb = bits <= 0 ? 0.0 : 1.0 / std::ldexp(1.0, bits - 1); }
 double process(double x) {
  if (lsb <= 0.0) return x;
  seed = seed * 1664525u + 1013904223u; const double a = static_cast<double>(seed >> 8) / 16777216.0;
  seed = seed * 1664525u + 1013904223u; const double b = static_cast<double>(seed >> 8) / 16777216.0;
  return std::round((x + (a - b) * lsb) / lsb) * lsb;   // triangular noise, then the word length
 }
private:
 double lsb = 0.0;
 std::uint32_t seed = 22222u;
};

} // namespace inverso_abismo::chain
