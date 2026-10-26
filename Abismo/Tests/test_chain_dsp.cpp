// INVERSO channel-strip / mastering blocks, measured: EQ response against the cookbook formulas,
// the saturator's loudness compensation and aliasing, the stereo width and bass mono, the
// compressor's static curve and time constants, the gate, the transient shaper, the dynamic band,
// the true-peak limiter on adversarial signals, the BS.1770 meter on the EBU reference tone.
#include "../Source/DSP/ChainDyn.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace inverso_abismo::chain;

static void verify(bool ok, const char* message) {
 if (!ok) { std::fprintf(stderr, "FAILED: %s\n", message); std::exit(1); }
}
static const double SR = 48000.0;

struct Stereo { std::vector<double> l, r; };
static Stereo sine(double f, double db, double seconds, double phase = 0.0, double sr = SR) {
 Stereo s; const auto n = static_cast<std::size_t>(seconds * sr);
 s.l.resize(n); s.r.resize(n);
 const double a = dbToGain(db);
 for (std::size_t i = 0; i < n; ++i) s.l[i] = s.r[i] = a * std::sin(2 * pi * f * static_cast<double>(i) / sr + phase);
 return s;
}
static double rmsDb(const std::vector<double>& x, std::size_t a, std::size_t b) {
 double s = 0; for (std::size_t i = a; i < b && i < x.size(); ++i) s += x[i] * x[i];
 return 10 * std::log10(s / static_cast<double>(b - a) + 1e-30);
}
static Stereo noise(double rmsDbfs, double seconds, std::uint32_t seed = 99) {
 Stereo s; const auto n = static_cast<std::size_t>(seconds * SR);
 s.l.resize(n); s.r.resize(n);
 double lp = 0, lp2 = 0; double acc = 0;
 for (std::size_t i = 0; i < n; ++i) {
  seed = seed * 1664525u + 1013904223u; const double w1 = static_cast<double>(seed >> 8) / 8388608.0 - 1.0;
  seed = seed * 1664525u + 1013904223u; const double w2 = static_cast<double>(seed >> 8) / 8388608.0 - 1.0;
  lp += .12 * (w1 - lp); lp2 += .05 * (lp - lp2);
  s.l[i] = lp * .7 + lp2 * 2.2; s.r[i] = lp * .7 + lp2 * 2.2 + .1 * w2;
  acc += s.l[i] * s.l[i];
 }
 const double k = dbToGain(rmsDbfs) / std::sqrt(acc / static_cast<double>(n));
 for (std::size_t i = 0; i < n; ++i) { s.l[i] *= k; s.r[i] *= k; }
 return s;
}

// ---------------------------------------------------------------- EQ
static void eq() {
 Eq e; e.prepare(SR);
 EqControls c; c.b1F = 1000; c.b1G = 6; c.b1Q = 1; c.hpf = 100; c.lsF = 100; c.lsG = -4; c.hsF = 8000; c.hsG = 3;
 e.setControls(c);
 for (double f : {50.0, 100.0, 250.0, 1000.0, 3000.0, 10000.0}) {
  Eq t; t.prepare(SR); t.setControls(c);
  double peak = 0; const auto s = sine(f, -20, .6);
  for (std::size_t i = 0; i < s.l.size(); ++i) { double l = s.l[i], r = s.r[i]; t.process(l, r); if (i > s.l.size() / 2) peak = std::max(peak, std::abs(l)); }
  const double measured = gainToDb(peak) + 20, expected = e.responseDb(f);
  std::printf("eq at %6.0f Hz: measured %+6.2f dB, formula %+6.2f dB\n", f, measured, expected);
  verify(std::abs(measured - expected) < .25, "EQ: measured response differs from the formula");
 }
 // flat EQ is bit-exact
 Eq flat; flat.prepare(SR); flat.setControls(EqControls{});
 double worst = 0; const auto s = noise(-18, .3);
 for (std::size_t i = 0; i < s.l.size(); ++i) { double l = s.l[i], r = s.r[i]; flat.process(l, r); worst = std::max(worst, std::max(std::abs(l - s.l[i]), std::abs(r - s.r[i]))); }
 verify(worst == 0.0, "EQ: flat must be bit-exact");
 // 4th-order high-pass: -3 dB at the corner, about -24 dB an octave below... (Butterworth)
 EqControls h; h.hpf = 200; Eq hp; hp.prepare(SR); hp.setControls(h);
 verify(std::abs(hp.responseDb(200) + 3.0) < .5 && hp.responseDb(100) < -22.0, "EQ: high-pass slope");
 // mid only / side only
 EqControls m; m.ms = 1; m.b1G = 12; m.b1F = 500; Eq me; me.prepare(SR); me.setControls(m);
 double sideIn = 0, sideOut = 0;
 for (int i = 0; i < 24000; ++i) { const double v = .3 * std::sin(2 * pi * 500 * i / SR); double l = v, r = -v; me.process(l, r); if (i > 12000) { sideIn += v * v; sideOut += (.5 * (l - r)) * (.5 * (l - r)); } }
 verify(std::abs(10 * std::log10(sideOut / sideIn)) < .2, "EQ: mid-only must leave the sides alone");
}

// ---------------------------------------------------------------- saturator
static void saturator() {
 for (int type = 0; type < 3; ++type) {
  double worstDb = 0;
  for (float drive : {10.f, 30.f, 60.f, 100.f}) {
   Saturator sat; sat.prepare(SR);
   SatControls c; c.type = static_cast<SatType>(type); c.drive = drive; sat.setControls(c);
   const auto in = noise(-18, 1.0, 5);
   double a = 0, b = 0;
   for (std::size_t i = 0; i < in.l.size(); ++i) { const double y = sat.process(0, in.l[i]); if (i > 8000) { a += in.l[i] * in.l[i]; b += y * y; } }
   const double d = 10 * std::log10(b / a);
   worstDb = std::max(worstDb, std::abs(d));
  }
  std::printf("saturator %d: loudness change at -18 dBFS across the drive range: at most %.2f dB\n", type, worstDb);
  verify(worstDb < 1.2, "saturator: the drive must be loudness-compensated");
 }
 // more drive, more harmonics (a 1 kHz tone: third harmonic relative to the fundamental)
 double thd[2];
 for (int k = 0; k < 2; ++k) {
  Saturator sat; sat.prepare(SR); SatControls c; c.type = SatType::Tape; c.drive = k ? 90.f : 15.f; sat.setControls(c);
  std::vector<double> y; const auto in = sine(1000, -10, .5);
  for (double v : in.l) y.push_back(sat.process(0, v));
  auto bin = [&](double f) { double re = 0, im = 0; for (std::size_t i = 12000; i < y.size(); ++i) { re += y[i] * std::cos(2 * pi * f * static_cast<double>(i) / SR); im += y[i] * std::sin(2 * pi * f * static_cast<double>(i) / SR); } return std::sqrt(re * re + im * im); };
  thd[k] = 20 * std::log10(bin(3000) / bin(1000));
 }
 std::printf("saturator third harmonic: %.1f dB at drive 15, %.1f dB at drive 90\n", thd[0], thd[1]);
 verify(thd[1] > thd[0] + 8.0, "saturator: more drive must bring more harmonics");
 // aliasing: a 7 kHz tone pushed hard must not fold back as inharmonic energy (anti-aliased shaper)
 {
  Saturator sat; sat.prepare(SR); SatControls c; c.type = SatType::Tape; c.drive = 100; sat.setControls(c);
  std::vector<double> y; const auto in = sine(7000, -6, .5);
  for (double v : in.l) y.push_back(sat.process(0, v));
  auto bin = [&](double f) { double re = 0, im = 0; for (std::size_t i = 12000; i < y.size(); ++i) { re += y[i] * std::cos(2 * pi * f * static_cast<double>(i) / SR); im += y[i] * std::sin(2 * pi * f * static_cast<double>(i) / SR); } return std::sqrt(re * re + im * im); };
  // 3rd harmonic 21 kHz is real; the 5th (35 kHz) folds to 13 kHz, the 7th (49 kHz) to 1 kHz
  const double alias = 20 * std::log10((bin(13000) + bin(1000)) / bin(7000));
  std::printf("saturator aliasing products: %.1f dB below the tone\n", alias);
  verify(alias < -30.0, "saturator: aliasing must be low");
 }
 // the tube leaves no DC
 {
  Saturator sat; sat.prepare(SR); SatControls c; c.type = SatType::Tube; c.drive = 80; sat.setControls(c);
  double mean = 0; int n = 0; const auto in = sine(200, -8, 1.5);
  for (std::size_t i = 0; i < in.l.size(); ++i) { const double y = sat.process(0, in.l[i]); if (i > 48000) { mean += y; ++n; } }
  verify(std::abs(mean / n) < .003, "saturator: the tube must not leave DC");
 }
}

// ---------------------------------------------------------------- width
static void width() {
 Width w; w.prepare(SR);
 WidthControls c; c.width = 0; w.setControls(c);
 double l = .7, r = -.2; w.process(l, r);
 verify(std::abs(l - r) < 1e-12 && std::abs(l - .25) < 1e-12, "width 0 must collapse to mono");
 c.width = 100; w.setControls(c); l = .7; r = -.2; w.process(l, r);
 verify(l == .7 && r == -.2, "width 100 must be the identity");
 // bass mono: a 50 Hz side signal goes, a 5 kHz side signal stays
 c.width = 100; c.bassMono = 150; w.setControls(c);
 for (double f : {50.0, 5000.0}) {
  Width t; t.prepare(SR); t.setControls(c);
  double in = 0, out = 0;
  for (int i = 0; i < 24000; ++i) { const double v = .3 * std::sin(2 * pi * f * i / SR); double a = v, b = -v; t.process(a, b); if (i > 12000) { in += v * v; out += .25 * (a - b) * (a - b); } }
  const double db = 10 * std::log10(out / in);
  std::printf("bass mono 150 Hz: side at %5.0f Hz changes by %.1f dB\n", f, db);
  verify(f < 100 ? db < -20 : std::abs(db) < .3, "width: bass mono");
 }
}

// ---------------------------------------------------------------- compressor
static double steadyGr(CompControls c, double levelDb) {
 Compressor k; k.prepare(SR); k.setControls(c);
 const auto in = sine(1000, levelDb, 2.5);
 double lastGr = 0;
 for (std::size_t i = 0; i < in.l.size(); ++i) { double l = in.l[i], r = in.r[i]; k.process(l, r); lastGr = k.grDb(); }
 return lastGr;
}
static void compressor() {
 CompControls c; c.type = CompType::Vca; c.thresh = -20; c.ratio = 4; c.knee = 0; c.attackMs = 5; c.releaseMs = 100; c.scHpf = 20; c.autoMakeup = false;
 const double gr = steadyGr(c, -8.0 - 3.0);   // a sine's RMS detector reads peak*(.5 share) ... compare to the static curve below
 Compressor ref; ref.prepare(SR); ref.setControls(c);
 // steady-state against staticGr evaluated at the level the detector sees (mix of peak and rms of a sine = peak)
 const double expected = ref.staticGr(-8.0 - 3.0);
 std::printf("vca gain reduction at -11 dBFS: %.2f dB, static curve %.2f dB\n", gr, expected);
 verify(std::abs(gr - expected) < .6, "compressor: steady gain reduction must follow the static curve");
 // below the threshold: nothing
 verify(steadyGr(c, -40) < .05, "compressor: no reduction under the threshold");
 // the variable-mu curve is softer near the threshold and keeps growing with the level
 CompControls m = c; m.type = CompType::VarMu; m.timeIdx = 0; m.knee = 6;
 const double g10 = steadyGr(m, -20 + 10 - 3), g30 = steadyGr(m, -20 + 30 - 3);
 std::printf("var-mu: %.2f dB reduction 10 dB over, %.2f dB 30 dB over (%.2f per dB then %.2f per dB)\n", g10, g30, g10 / 10, (g30 - g10) / 20);
 verify((g30 - g10) / 20 > g10 / 10 * 1.15, "variable-mu: the ratio must rise with the level");
 // attack: time to reach 63% of the final reduction after a step
 for (double att : {2.0, 10.0, 40.0}) {
  CompControls a = c; a.attackMs = static_cast<float>(att); a.releaseMs = 500;
  Compressor k; k.prepare(SR); k.setControls(a);
  const double final = steadyGr(a, -8.0 - 3.0);
  const auto in = sine(1000, -11, .5);
  int hit = -1;
  for (std::size_t i = 0; i < in.l.size(); ++i) { double l = in.l[i], r = in.r[i]; k.process(l, r); if (hit < 0 && k.grDb() >= final * .63) hit = static_cast<int>(i); }
  const double ms = hit * 1000.0 / SR;
  std::printf("attack %.0f ms: 63%% of the reduction after %.1f ms\n", att, ms);
  verify(ms > att * .45 && ms < att * 2.2 + 1.0, "compressor: the attack time must match the knob");
 }
 // release: after the burst the reduction falls to 37% in about the release time
 for (double rel : {50.0, 300.0}) {
  CompControls a = c; a.attackMs = 1; a.releaseMs = static_cast<float>(rel);
  Compressor k; k.prepare(SR); k.setControls(a);
  const auto loud = sine(1000, -11, 1.0), quiet = sine(1000, -60, 2.0);
  double top = 0;
  for (std::size_t i = 0; i < loud.l.size(); ++i) { double l = loud.l[i], r = loud.r[i]; k.process(l, r); top = std::max(top, k.grDb()); }
  int hit = -1;
  for (std::size_t i = 0; i < quiet.l.size(); ++i) { double l = quiet.l[i], r = quiet.r[i]; k.process(l, r); if (hit < 0 && k.grDb() <= top * .37) hit = static_cast<int>(i); }
  const double ms = hit * 1000.0 / SR;
  std::printf("release %.0f ms: down to 37%% after %.1f ms\n", rel, ms);
  verify(ms > rel * .6 && ms < rel * 1.8 + 5.0, "compressor: the release time must match the knob");
 }
 // parallel: mix 0 is the dry signal exactly
 CompControls p = c; p.mix = 0;
 Compressor k; k.prepare(SR); k.setControls(p);
 const auto in = noise(-10, .3); double worst = 0;
 for (std::size_t i = 0; i < in.l.size(); ++i) { double l = in.l[i], r = in.r[i]; k.process(l, r); worst = std::max(worst, std::abs(l - in.l[i])); }
 verify(worst < 1e-12, "compressor: mix 0 must be the dry signal");
 // auto make-up keeps the average level within about 3 dB
 for (int type = 0; type < 4; ++type) {
  CompControls a; a.type = static_cast<CompType>(type); a.thresh = -24; a.ratio = 4; a.autoMakeup = true; a.timeIdx = 1; a.knee = 6;
  Compressor kk; kk.prepare(SR); kk.setControls(a);
  const auto n = noise(-20, 4.0, 77); double ia = 0, oa = 0;
  for (std::size_t i = 0; i < n.l.size(); ++i) { double l = n.l[i], r = n.r[i]; kk.process(l, r); if (i > 24000) { ia += n.l[i] * n.l[i]; oa += l * l; } }
  const double d = 10 * std::log10(oa / ia);
  std::printf("compressor type %d: auto make-up leaves the level %+.2f dB from the input\n", type, d);
  verify(std::abs(d) < 3.5, "compressor: auto make-up out of range");
 }
}

// ---------------------------------------------------------------- gate
static void gate() {
 GateControls c; c.on = true; c.thresh = -30; c.range = 40; c.attackMs = .3f; c.holdMs = 50; c.releaseMs = 80; Gate g; g.prepare(SR); g.setControls(c);
 std::vector<double> out;
 for (int i = 0; i < 48000; ++i) {
  const double v = (i > 12000 && i < 18000) ? .5 * std::sin(2 * pi * 150 * i / SR) : .002 * std::sin(2 * pi * 150 * i / SR);   // a hit, then a quiet bleed 50 dB down
  double l = v, r = v; g.process(l, r); out.push_back(l);
 }
 const double during = rmsDb(out, 13000, 17000), after = rmsDb(out, 40000, 48000), bleed = -50.0 - 3;
 std::printf("gate: hit passes at %.1f dB (in %.1f), the bleed afterwards is cut to %.1f dB\n", during, 20 * std::log10(.5 / std::sqrt(2.0)), after);
 verify(std::abs(during - 20 * std::log10(.5 / std::sqrt(2.0))) < .3, "gate: the hit must pass untouched");
 verify(after < bleed - 30, "gate: the bleed must be cut by the range");
 // transparent when off
 GateControls off; Gate g2; g2.prepare(SR); g2.setControls(off); double l = .3, r = .2; g2.process(l, r);
 verify(l == .3 && r == .2, "gate: off is bit-exact");
}

// ---------------------------------------------------------------- transient shaper
static Stereo drumHits(double seconds) {
 Stereo s; const auto n = static_cast<std::size_t>(seconds * SR); s.l.assign(n, 0); s.r.assign(n, 0);
 for (double t = .05; t < seconds - .3; t += .5) {
  const auto at = static_cast<std::size_t>(t * SR);
  for (std::size_t k = 0; k < static_cast<std::size_t>(.3 * SR) && at + k < n; ++k) {
   const double e = std::exp(-static_cast<double>(k) / (.08 * SR));
   const double v = (.6 * std::exp(-static_cast<double>(k) / (.004 * SR)) + .25 * e) * std::sin(2 * pi * 80 * static_cast<double>(k) / SR);
   s.l[at + k] = s.r[at + k] = v;
  }
 }
 return s;
}
static void transient() {
 const auto in = drumHits(3.0);
 auto run = [&](float attack, float sustain) {
  TransControls c; c.on = true; c.attack = attack; c.sustain = sustain; Transient t; t.prepare(SR); t.setControls(c);
  Stereo o = in;
  for (std::size_t i = 0; i < in.l.size(); ++i) { double l = in.l[i], r = in.r[i]; t.process(l, r); o.l[i] = l; }
  return o;
 };
 auto peakOf = [&](const Stereo& s, std::size_t a, std::size_t b) { double p = 0; for (std::size_t i = a; i < b; ++i) p = std::max(p, std::abs(s.l[i])); return gainToDb(p); };
 // the second hit's onset window and tail window
 const std::size_t h = static_cast<std::size_t>(.55 * SR);
 const auto flat = run(0, 0), up = run(100, 0), down = run(-100, 0), sus = run(0, 100), susDown = run(0, -100);
 double worst = 0; for (std::size_t i = 0; i < in.l.size(); ++i) worst = std::max(worst, std::abs(flat.l[i] - in.l[i]));
 verify(worst < 1e-9, "transient: 0/0 must be the identity");
 const double onset = peakOf(up, h, h + 480) - peakOf(flat, h, h + 480), onsetDown = peakOf(down, h, h + 480) - peakOf(flat, h, h + 480);
 const double tail = rmsDb(sus.l, h + 6000, h + 10000) - rmsDb(flat.l, h + 6000, h + 10000), tailDown = rmsDb(susDown.l, h + 6000, h + 10000) - rmsDb(flat.l, h + 6000, h + 10000);
 std::printf("transient: attack +100 -> onset %+.1f dB, -100 -> %+.1f dB; sustain +100 -> tail %+.1f dB, -100 -> %+.1f dB\n", onset, onsetDown, tail, tailDown);
 verify(onset > 3.0 && onsetDown < -3.0 && tail > 3.0 && tailDown < -3.0, "transient: the controls must act on onsets and tails");
}

// ---------------------------------------------------------------- dynamic band
static void tame() {
 TameControls c; c.on = true; c.freq = 6500; c.thresh = -30; c.range = 10; c.q = 2;
 auto gainAt = [&](double f, double db) {
  Tame t; t.prepare(SR); t.setControls(c);
  const auto s = sine(f, db, 1.0); double p = 0;
  for (std::size_t i = 0; i < s.l.size(); ++i) { double l = s.l[i], r = s.r[i]; t.process(l, r); if (i > 24000) p = std::max(p, std::abs(l)); }
  return gainToDb(p) - db;
 };
 const double hot = gainAt(6500, -12), quiet = gainAt(6500, -60), away = gainAt(200, -12);
 std::printf("tame: 6.5 kHz at -12 dBFS %+.2f dB, at -60 dBFS %+.2f dB, 200 Hz %+.2f dB\n", hot, quiet, away);
 verify(hot < -5.0 && hot > -10.6 && std::abs(quiet) < .2 && std::abs(away) < .2, "tame: only the band over the threshold is reduced, never more than RANGE");
}

// ---------------------------------------------------------------- limiter
// True peak by an independent 8x windowed-sinc reconstruction.
static double truePeakDb(const std::vector<double>& x) {
 const int K = 32, R = 8; double peak = 0;
 for (std::size_t n = K; n + K < x.size(); ++n) {
  peak = std::max(peak, std::abs(x[n]));
  for (int p = 1; p < R; ++p) {
   double a = 0;
   for (int k = -K + 1; k <= K; ++k) { const double t = k - static_cast<double>(p) / R; const double w = .5 + .5 * std::cos(pi * t / (K + .5)); a += x[static_cast<std::size_t>(static_cast<long>(n) + k)] * (std::sin(pi * t) / (pi * t)) * w; }
   peak = std::max(peak, std::abs(a));
  }
 }
 return gainToDb(peak);
}
static void limiter() {
 LimiterControls c; c.ceilingDb = -1.0; c.gainDb = 0; c.autoRelease = true;
 // 1. the ceiling holds on hard material: noise 12 dB over, a full-scale fs/4 tone with its worst phase, a drum loop pushed
 struct Case { const char* name; Stereo s; double pushDb; };
 std::vector<Case> cases;
 cases.push_back({"noise +12 dB", noise(-6, 3.0), 12.0});
 cases.push_back({"fs/4 tone, 45 degrees, 0 dBFS", sine(SR / 4, 0.0, 1.0, pi / 4), 0.0});
 cases.push_back({"drums +15 dB", drumHits(3.0), 15.0});
 cases.push_back({"10 kHz tone +6 dB", sine(10000, -2.0, 1.0), 8.0});
 for (auto& cs : cases) {
  Limiter lim; lim.prepare(SR); LimiterControls k = c; k.gainDb = static_cast<float>(cs.pushDb); lim.setControls(k);
  std::vector<double> out; out.reserve(cs.s.l.size());
  for (std::size_t i = 0; i < cs.s.l.size(); ++i) { double l = cs.s.l[i], r = cs.s.r[i]; lim.process(l, r); out.push_back(l); }
  const double tp = truePeakDb(out);
  std::printf("limiter, %-30s true peak %+.2f dBTP (ceiling -1.00)\n", cs.name, tp);
  verify(tp < -1.0 + .15, "limiter: the true peak must stay under the ceiling");
 }
 // 2. below the ceiling it is transparent apart from the delay, and the delay is the reported latency
 {
  Limiter lim; lim.prepare(SR); lim.setControls(c);
  const auto in = noise(-30, 1.0, 3); double worst = 0;
  std::vector<double> out;
  for (std::size_t i = 0; i < in.l.size(); ++i) { double l = in.l[i], r = in.r[i]; lim.process(l, r); out.push_back(l); }
  const auto d = static_cast<std::size_t>(lim.latency());
  for (std::size_t i = d + 10; i < in.l.size(); ++i) worst = std::max(worst, std::abs(out[i] - in.l[i - d]));
  std::printf("limiter below the ceiling: largest difference to the delayed input %.2e (latency %d samples)\n", worst, lim.latency());
  verify(worst < 1e-12, "limiter: transparent below the ceiling, delayed by exactly its latency");
 }
 // 2b. the reported latency is the real delay at every sample rate (the host compensates by it)
 for (double rate : {44100.0, 48000.0, 88200.0, 96000.0, 192000.0}) {
  Limiter lim; lim.prepare(rate); lim.setControls(c);
  const int n = static_cast<int>(rate * .2);
  int firstOut = -1; double peak = 0;
  for (int i = 0; i < n; ++i) {
   double l = i == 1000 ? .2 : 0.0, r = l;
   lim.process(l, r);
   if (std::abs(l) > peak * .999 && std::abs(l) > .01 && firstOut < 0) firstOut = i;
   peak = std::max(peak, std::abs(l));
  }
  std::printf("limiter at %.0f Hz: impulse delayed by %d samples, latency() says %d\n", rate, firstOut - 1000, lim.latency());
  verify(firstOut - 1000 == lim.latency(), "limiter: the reported latency must be the real delay at every sample rate");
 }
 // 3. steady loud tone: the reduction is steady (no pumping) and no more than needed
 {
  Limiter lim; lim.prepare(SR); LimiterControls k = c; k.gainDb = 9; lim.setControls(k);
  const auto in = sine(997, -3, 2.0); std::vector<double> gr;
  for (std::size_t i = 0; i < in.l.size(); ++i) { double l = in.l[i], r = in.r[i]; lim.process(l, r); if (i > 24000) gr.push_back(lim.grDb()); }
  double mn = 1e9, mx = -1e9; for (double g : gr) { mn = std::min(mn, g); mx = std::max(mx, g); }
  std::printf("limiter on a steady tone: reduction between %.2f and %.2f dB (need %.2f)\n", mn, mx, -3 + 9 + 1.0);
  verify(mx - mn < .35 && std::abs(mx - (-3 + 9 + 1.0)) < .6, "limiter: steady, and no more reduction than needed");
 }
 // 4. recovery: after a loud burst the gain is back to 0 dB
 {
  Limiter lim; lim.prepare(SR); LimiterControls k = c; k.gainDb = 12; lim.setControls(k);
  double last = 99;
  for (int i = 0; i < 96000; ++i) { const double v = i < 4800 ? .9 * std::sin(2 * pi * 100 * i / SR) : .001 * std::sin(2 * pi * 100 * i / SR); double l = v, r = v; lim.process(l, r); last = lim.grDb(); }
  verify(last < .3, "limiter: the gain must recover after the burst");
 }
}

// ---------------------------------------------------------------- loudness
static void loudness() {
 // EBU Tech 3341: a stereo 997 Hz sine at -23 dBFS peak in both channels reads -23.0 LUFS
 Loudness m; m.prepare(SR);
 const auto s = sine(997, -23, 25.0);
 for (std::size_t i = 0; i < s.l.size(); ++i) m.process(s.l[i], s.r[i]);
 std::printf("loudness: stereo 997 Hz at -23 dBFS: momentary %.2f, short-term %.2f, integrated %.2f LUFS\n", m.momentaryLufs(), m.shortTermLufs(), m.integratedLufs());
 verify(std::abs(m.integratedLufs() + 23.0) < .15 && std::abs(m.shortTermLufs() + 23.0) < .15 && std::abs(m.momentaryLufs() + 23.0) < .15, "loudness: the reference tone must read -23 LUFS");
 // gating: a loud section then a long silence: the integrated value ignores the silence
 Loudness g; g.prepare(SR);
 const auto loud = sine(997, -23, 10.0);
 for (std::size_t i = 0; i < loud.l.size(); ++i) g.process(loud.l[i], loud.r[i]);
 const double before = g.integratedLufs();
 for (int i = 0; i < 48000 * 10; ++i) g.process(0, 0);
 std::printf("loudness: integrated %.2f before and %.2f after ten seconds of silence\n", before, g.integratedLufs());
 verify(std::abs(g.integratedLufs() - before) < .1, "loudness: the gate must ignore silence");
 // a tone 10 dB lower than half of the programme is gated out by the relative gate
 Loudness r; r.prepare(SR);
 const auto a = sine(997, -23, 20.0), b = sine(997, -43, 6.0);
 for (std::size_t i = 0; i < a.l.size(); ++i) r.process(a.l[i], a.r[i]);
 for (std::size_t i = 0; i < b.l.size(); ++i) r.process(b.l[i], b.r[i]);
 verify(std::abs(r.integratedLufs() + 23.0) < .3, "loudness: the relative gate");
}

// ---------------------------------------------------------------- dither
static void dither() {
 Dither d; d.setBits(16);
 const double lsb = 1.0 / 32768.0; double worst = 0;
 for (int i = 0; i < 20000; ++i) { const double x = .3 * std::sin(i * .01); const double y = d.process(x); worst = std::max(worst, std::abs(y / lsb - std::round(y / lsb))); verify(std::abs(y - x) <= lsb * 1.51, "dither: error within 1.5 steps (TPDF plus rounding)"); }
 verify(worst < 1e-6, "dither: the output sits on the 16-bit grid");
 Dither off; off.setBits(0); verify(off.process(.123456789) == .123456789, "dither: off is exact");
}

int main() {
 eq(); saturator(); width(); compressor(); gate(); transient(); tame(); limiter(); loudness(); dither();
 std::puts("chain dsp: all good");
 return 0;
}
