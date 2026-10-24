// INVERSO VST3 conformance: the built bundle, loaded the way a DAW loads it (JUCE's VST3 host),
// through everything a session can do to it. Every check is deterministic (fixed seeds), so a
// pass means the same thing every time. Exit code 0 only if every check passes.
//
//  1. programs     every program loads, names are set, the current program is reported back
//  2. state        pluginval's sequence (programs, audio, plugin state, restoration per
//                  parameter) over many seeds; the state read back is byte-identical to the
//                  state loaded; a fresh instance loaded with a state reads the same values
//  3. audio        every program at 44.1/48/88.2/96/192 kHz, block sizes 1..2048 and uneven
//                  blocks; output finite, no denormals, within +12 dBFS; instruments play notes
//                  and fall silent after note-off (unless the program holds or freezes)
//  4. lifecycle    prepare/release cycles, prepare at a new rate without release, reset,
//                  bypass, zero-length blocks
//  5. fuzz         random parameter values every block while playing; output stays sane
//  6. threads      parameters changed from another thread while audio runs; state saved and
//                  loaded from another thread while audio runs
//  7. editor       opened, resized and closed while audio runs on another thread
//  8. cpu          default program, 8 notes, 48 kHz / 512: reports the share of one core
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <thread>

namespace {
int failures = 0;
juce::String pluginName;
void fail(const juce::String& what) {
 ++failures;
 if (failures <= 40) std::printf("  FAIL %s: %s\n", pluginName.toRawUTF8(), what.toRawUTF8());
}

struct Sanity {
 float peak = 0.f;
 int nonFinite = 0, denormal = 0;
 void scan(const juce::AudioBuffer<float>& b, int n) {
  for (int c = 0; c < b.getNumChannels(); ++c)
   for (int i = 0; i < n; ++i) {
    const float x = b.getSample(c, i);
    if (!std::isfinite(x)) { ++nonFinite; continue; }
    if (std::fpclassify(x) == FP_SUBNORMAL) ++denormal;
    peak = std::max(peak, std::abs(x));
   }
 }
 bool check(const juce::String& where, float limit = 4.f) const { // 4 = +12 dBFS
  bool ok = true;
  if (nonFinite) { fail(where + ": " + juce::String(nonFinite) + " NaN/inf samples"); ok = false; }
  if (denormal) { fail(where + ": " + juce::String(denormal) + " denormal samples"); ok = false; }
  if (peak > limit) { fail(where + ": peak " + juce::String(juce::Decibels::gainToDecibels(peak), 1) + " dBFS"); ok = false; }
  return ok;
 }
};

constexpr float runaway = 1000.f; // +60 dBFS: past any setting a plugin allows, so only a blow-up gets here

int channelsOf(juce::AudioPluginInstance& p) { return std::max(2, std::max(p.getTotalNumInputChannels(), p.getTotalNumOutputChannels())); }

// a signal for effects: a chord of sines with an envelope, at a realistic level
void fillInput(juce::AudioBuffer<float>& b, int n, int64_t& t, double sr, juce::AudioPluginInstance& p) {
 b.clear();
 const int ins = p.getTotalNumInputChannels();
 for (int i = 0; i < n; ++i, ++t) {
  const double s = static_cast<double>(t) / sr;
  const double env = std::fmod(s, 1.0) < .6 ? 1.0 : 0.0;
  const float x = static_cast<float>(.25 * env * (std::sin(6.2831853 * 110 * s) + .5 * std::sin(6.2831853 * 440 * s) + .25 * std::sin(6.2831853 * 3520 * s)));
  for (int c = 0; c < ins; ++c) b.setSample(c, i, x);
 }
}

std::unique_ptr<juce::AudioPluginInstance> load(juce::AudioPluginFormatManager& fm, const juce::PluginDescription& d, double sr, int block) {
 juce::String err;
 auto p = fm.createPluginInstance(d, sr, block, err);
 if (!p) fail("could not load: " + err);
 return p;
}

// renders `seconds`, notes on for the first `held` seconds when the plugin takes MIDI; returns sanity
Sanity render(juce::AudioPluginInstance& p, double sr, int block, double seconds, double held, bool uneven = false, int seed = 1,
              std::function<void(int)> everyBlock = {}) {
 Sanity s;
 juce::AudioBuffer<float> b(channelsOf(p), block);
 juce::MidiBuffer midi;
 juce::Random r(seed);
 int64_t t = 0;
 const int64_t total = static_cast<int64_t>(seconds * sr), offAt = static_cast<int64_t>(held * sr);
 const bool instrument = p.acceptsMidi();
 bool notesOn = false, notesOff = false;
 int k = 0;
 while (t < total) {
  const int n = uneven ? 1 + r.nextInt(block) : block;
  midi.clear();
  if (instrument && !notesOn) { for (int note : {36, 48, 55, 60, 64, 67, 72, 76}) midi.addEvent(juce::MidiMessage::noteOn(1, note, .8f), 0); notesOn = true; }
  if (instrument && !notesOff && t + n > offAt) { for (int note : {36, 48, 55, 60, 64, 67, 72, 76}) midi.addEvent(juce::MidiMessage::noteOff(1, note), 0); notesOff = true; }
  if (everyBlock) everyBlock(k++);
  int64_t tt = t;
  fillInput(b, n, tt, sr, p);
  juce::AudioBuffer<float> view(b.getArrayOfWritePointers(), b.getNumChannels(), n);
  p.processBlock(view, midi);
  s.scan(view, n);
  t += n;
 }
 return s;
}

float tailPeak(juce::AudioPluginInstance& p, double sr, int block, double seconds) {
 juce::AudioBuffer<float> b(channelsOf(p), block);
 juce::MidiBuffer midi;
 float peak = 0.f;
 for (int64_t t = 0; t < static_cast<int64_t>(seconds * sr); t += block) {
  b.clear();
  p.processBlock(b, midi);
  if (t > static_cast<int64_t>((seconds - .5) * sr)) peak = std::max(peak, b.getMagnitude(0, block));
 }
 return peak;
}

// the switches that make a sound keep going after note-off on purpose (exact names, from the Params headers)
bool holdsOrFreezes(juce::AudioPluginInstance& p) {
 static const juce::StringArray holding{"Reverb freeze", "Latch (keep playing after release)", "One-shot (ignore note off)"};
 for (auto* q : p.getParameters())
  if (holding.contains(q->getName(64)) && q->getValue() >= .5f) return true;
 return false;
}

void programsCheck(juce::AudioPluginInstance& p) {
 const int n = p.getNumPrograms();
 for (int i = 0; i < n; ++i) {
  p.setCurrentProgram(i);
  if (p.getProgramName(i).trim().isEmpty()) fail("program " + juce::String(i) + " has no name");
  if (p.getCurrentProgram() != i) fail("program " + juce::String(i) + " selected but " + juce::String(p.getCurrentProgram()) + " reported");
 }
}

void stateChecks(juce::AudioPluginFormatManager& fm, const juce::PluginDescription& d, int seeds) {
 int runsFailed = 0;
 for (int seed = 1; seed <= seeds; ++seed) {
  auto p = load(fm, d, 44100, 64);
  if (!p) return;
  juce::Random r(seed);
  const int n = p->getNumPrograms(), cur = p->getCurrentProgram();
  for (int i = 0; i < 5 && n > 0; ++i) p->setCurrentProgram(r.nextInt(n));
  if (n > 0 && cur >= 0) p->setCurrentProgram(cur);
  p->prepareToPlay(44100, 64);
  render(*p, 44100, 64, .2, .1);
  { juce::MemoryBlock s; p->getStateInformation(s);
    for (auto* q : p->getParameters()) if (q->isAutomatable()) q->setValue(r.nextFloat());
    p->setStateInformation(s.getData(), static_cast<int>(s.getSize())); }
  juce::MemoryBlock s; p->getStateInformation(s);
  bool bad = false;
  for (auto* q : p->getParameters()) {
   if (!q->isAutomatable()) continue;
   const float o = q->getValue();
   q->setValue(r.nextFloat());
   p->setStateInformation(s.getData(), static_cast<int>(s.getSize()));
   if (std::abs(q->getValue() - o) > .1f) {
    if (!bad) fail("seed " + juce::String(seed) + ": " + q->getName(40) + " not restored (wanted " + juce::String(o, 3) + ", got " + juce::String(q->getValue(), 3) + ")");
    bad = true;
   }
  }
  juce::MemoryBlock again; p->getStateInformation(again);
  if (!(again == s)) { fail("seed " + juce::String(seed) + ": state read back differs from the state loaded"); bad = true; }
  // a fresh instance loaded with this state reads the same values (a session reopened)
  { std::vector<float> want; for (auto* q : p->getParameters()) want.push_back(q->getValue());
    auto fresh = load(fm, d, 44100, 64);
    if (fresh) {
     fresh->setStateInformation(s.getData(), static_cast<int>(s.getSize()));
     auto& fp = fresh->getParameters();
     for (int i = 0; i < fp.size() && i < static_cast<int>(want.size()); ++i)
      if (std::abs(fp[i]->getValue() - want[static_cast<std::size_t>(i)]) > 1e-4f) {
       fail("seed " + juce::String(seed) + ": reopened session has " + fp[i]->getName(40) + " at " + juce::String(fp[i]->getValue(), 3) + " instead of " + juce::String(want[static_cast<std::size_t>(i)], 3));
       bad = true; break;
      }
    } }
  runsFailed += bad;
 }
 std::printf("  state: %d seeds, %d failed\n", seeds, runsFailed);
}

void audioChecks(juce::AudioPluginInstance& p) {
 const int n = std::max(1, p.getNumPrograms());
 int checked = 0;
 // every program at 48 kHz / 512, with a release check for instruments
 for (int prog = 0; prog < n; ++prog) {
  if (p.getNumPrograms() > 0) p.setCurrentProgram(prog);
  p.prepareToPlay(48000, 512);
  const auto s = render(p, 48000, 512, 2.5, 1.5);
  s.check("program " + juce::String(prog) + " (" + p.getProgramName(prog) + ")");
  if (p.acceptsMidi() && !holdsOrFreezes(p)) {
   const float tail = tailPeak(p, 48000, 512, 20.0);
   if (tail > 1.0e-3f) fail("program " + juce::String(prog) + " (" + p.getProgramName(prog) + ") still sounds 20 s after note-off (" + juce::String(juce::Decibels::gainToDecibels(tail), 1) + " dBFS)");
  }
  p.releaseResources();
  ++checked;
 }
 // rates and block sizes, steady and uneven
 if (p.getNumPrograms() > 0) p.setCurrentProgram(0);
 for (double sr : {44100.0, 48000.0, 88200.0, 96000.0, 192000.0})
  for (int block : {1, 7, 64, 333, 512, 2048}) {
   p.prepareToPlay(sr, block);
   const double seconds = block == 1 ? .15 : .8;
   render(p, sr, block, seconds, seconds * .6).check(juce::String(sr, 0) + " Hz / " + juce::String(block));
   render(p, sr, block, seconds, seconds * .6, true).check(juce::String(sr, 0) + " Hz / uneven up to " + juce::String(block));
   p.releaseResources();
   ++checked;
  }
 std::printf("  audio: %d programs and %d rate/block combinations\n", n, checked - n);
}

void lifecycleChecks(juce::AudioPluginInstance& p) {
 for (int i = 0; i < 3; ++i) { p.prepareToPlay(44100, 256); render(p, 44100, 256, .3, .2).check("prepare/release cycle"); p.releaseResources(); }
 p.prepareToPlay(44100, 256); render(p, 44100, 256, .2, .1);
 p.prepareToPlay(96000, 1024); render(p, 96000, 1024, .3, .2).check("new rate without release");
 p.reset(); render(p, 96000, 1024, .3, .2).check("after reset");
 { juce::AudioBuffer<float> b(channelsOf(p), 0); juce::MidiBuffer m; p.processBlock(b, m); } // zero-length block
 { juce::AudioBuffer<float> b(channelsOf(p), 1024); juce::MidiBuffer m; b.clear();
   // bypassed, an effect passes its input through untouched; an instrument stays sane
   int64_t t = 0; float worst = 0.f; Sanity s;
   for (int i = 0; i < 20; ++i) {
    fillInput(b, 1024, t, 96000, p);
    juce::AudioBuffer<float> in; in.makeCopyOf(b);
    p.processBlockBypassed(b, m);
    s.scan(b, 1024);
    if (!p.acceptsMidi())
     for (int c = 0; c < std::min(p.getTotalNumInputChannels(), p.getTotalNumOutputChannels()); ++c)
      for (int k = 0; k < 1024; ++k) worst = std::max(worst, std::abs(b.getSample(c, k) - in.getSample(c, k)));
   }
   s.check("bypassed");
   if (worst > 1.0e-6f) fail("bypassed effect changes its input (by up to " + juce::String(worst, 6) + ")"); }
 p.releaseResources();
 std::printf("  lifecycle: done\n");
}

void fuzzChecks(juce::AudioPluginInstance& p) {
 for (int seed = 1; seed <= 6; ++seed) {
  if (p.getNumPrograms() > 0) p.setCurrentProgram(seed % p.getNumPrograms());
  p.prepareToPlay(48000, 256);
  juce::Random r(seed * 7919);
  auto& params = p.getParameters();
  const auto s = render(p, 48000, 256, 4.0, 3.0, seed % 2 == 0, seed, [&](int) {
   for (int k = 0; k < 4; ++k) if (params.size()) params[r.nextInt(params.size())]->setValue(r.nextFloat());
  });
  // any setting at all, so loud is allowed (TIDE alone reaches +32 dBFS with every band boosted);
  // what is not: NaN/inf, denormals, or a runaway past +60 dBFS
  s.check("fuzz seed " + juce::String(seed), runaway);
  p.releaseResources();
 }
 std::printf("  fuzz: 6 seeds\n");
}

void threadChecks(juce::AudioPluginInstance& p, bool withEditor) {
 if (p.getNumPrograms() > 0) p.setCurrentProgram(0);
 p.prepareToPlay(48000, 256);
 std::atomic<bool> run{true};
 std::atomic<int> bad{0}, blocks{0}, badNonFinite{0};
 std::atomic<float> badPeak{0.f};
 std::thread audio([&] {
  juce::AudioBuffer<float> b(channelsOf(p), 256);
  juce::MidiBuffer midi;
  int64_t t = 0;
  int k = 0;
  while (run.load()) {
   midi.clear();
   if (p.acceptsMidi() && k % 40 == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 48 + k / 40 % 24, .8f), 0);
   if (p.acceptsMidi() && k % 40 == 30) midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
   fillInput(b, 256, t, 48000, p);
   p.processBlock(b, midi);
   Sanity s; s.scan(b, 256);
   if (s.nonFinite || s.peak > runaway) { ++bad; badNonFinite += s.nonFinite; badPeak = std::max(badPeak.load(), s.peak); }
   ++k;
   ++blocks;
  }
 });
 while (blocks.load() < 4) std::this_thread::yield(); // the audio thread is running before anything changes
 juce::Random r(99);
 auto& params = p.getParameters();
 const int before = blocks.load();
 const double until = juce::Time::getMillisecondCounterHiRes() + 1500.0; // at least 1.5 s of overlap
 for (int i = 0; i < 4000 || juce::Time::getMillisecondCounterHiRes() < until; ++i) {
  if (params.size()) params[r.nextInt(params.size())]->setValueNotifyingHost(r.nextFloat());
  if (i % 200 == 0) {
   juce::MemoryBlock s; p.getStateInformation(s);
   p.setStateInformation(s.getData(), static_cast<int>(s.getSize()));
   if (p.getNumPrograms() > 0) p.setCurrentProgram((i / 200) % p.getNumPrograms());
  }
  if (i % 50 == 0) std::this_thread::sleep_for(std::chrono::microseconds(200));
 }
 if (blocks.load() - before < 50) fail("audio thread processed only " + juce::String(blocks.load() - before) + " blocks while parameters and state changed");
 if (withEditor)
  for (int i = 0; i < 3; ++i) {
   // on screen, as a DAW shows it: only then does the plugin build its real editor
   std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditorAndMakeActive());
   if (!ed) { fail("no editor"); break; }
   const int w = ed->getWidth(), h = ed->getHeight();
   if (w < 50 || h < 50) fail("editor size " + juce::String(w) + " x " + juce::String(h));
   ed->addToDesktop(0);
   ed->setVisible(true);
   juce::MessageManager::getInstance()->runDispatchLoopUntil(400);
   if (ed->isResizable()) ed->setSize(w * 4 / 5, h * 4 / 5);
   juce::MessageManager::getInstance()->runDispatchLoopUntil(300);
   if (ed->isResizable()) ed->setSize(w * 6 / 5, h * 6 / 5);
   juce::MessageManager::getInstance()->runDispatchLoopUntil(300);
   // hidden, then the queue drained before the window goes: JUCE's Linux host (XEmbed) posts
   // callAsync([this]) on every resize with no lifetime guard, and deleting the window with one
   // still queued is a use-after-free in the host, not in the plugin (ASan showed it)
   ed->setVisible(false);
   juce::MessageManager::getInstance()->runDispatchLoopUntil(300);
   ed.reset();
  }
 run = false;
 audio.join();
 if (bad.load()) fail(juce::String(bad.load()) + " broken blocks while other threads changed parameters, state and editor ("
                     + juce::String(badNonFinite.load()) + " NaN/inf samples, peak " + juce::String(juce::Decibels::gainToDecibels(badPeak.load()), 1) + " dBFS)");
 p.releaseResources();
 std::printf("  threads%s: done\n", withEditor ? " + editor" : "");
}

void cpuReport(juce::AudioPluginInstance& p) {
 if (p.getNumPrograms() > 0) p.setCurrentProgram(0);
 p.prepareToPlay(48000, 512);
 render(p, 48000, 512, .5, .4);
 const auto t0 = juce::Time::getMillisecondCounterHiRes();
 render(p, 48000, 512, 10.0, 9.0);
 const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
 std::printf("  cpu: %.1f%% of one core (48 kHz / 512, 8 notes held)\n", ms / 100.0);
 p.releaseResources();
}
} // namespace

int main(int argc, char** argv) {
 if (argc < 2) { std::puts("usage: InversoVst3Conformance <bundle.vst3> [state seeds] [--no-editor] (any order)"); return 2; }
 std::setvbuf(stdout, nullptr, _IONBF, 0); // a crash must not swallow what was already reported
 juce::ScopedJuceInitialiser_GUI gui;
 juce::AudioPluginFormatManager fm;
 fm.addFormat(std::make_unique<juce::VST3PluginFormat>());
 juce::VST3PluginFormat vst3;
 juce::OwnedArray<juce::PluginDescription> descs;
 vst3.findAllTypesForFile(descs, argv[1]);
 if (descs.isEmpty()) { std::printf("no VST3 plugin in %s\n", argv[1]); return 2; }
 const auto& d = *descs[0];
 pluginName = d.name;
 int seeds = 30;
 bool editor = true;
 for (int i = 2; i < argc; ++i) {
  const juce::String a(argv[i]);
  if (a == "--no-editor") editor = false;
  else if (a.containsOnly("0123456789") && a.getIntValue() > 0) seeds = a.getIntValue();
  else { std::printf("unknown argument: %s\n", argv[i]); return 2; }
 }
 std::printf("%s (%s)\n", d.name.toRawUTF8(), d.isInstrument ? "instrument" : "effect");
 if (auto p = load(fm, d, 48000, 512)) {
  programsCheck(*p);
  std::printf("  programs: %d\n", p->getNumPrograms());
 }
 stateChecks(fm, d, seeds);
 if (auto p = load(fm, d, 48000, 512)) audioChecks(*p);
 if (auto p = load(fm, d, 48000, 512)) lifecycleChecks(*p);
 if (auto p = load(fm, d, 48000, 512)) fuzzChecks(*p);
 if (auto p = load(fm, d, 48000, 512)) threadChecks(*p, editor);
 if (auto p = load(fm, d, 48000, 512)) cpuReport(*p);
 std::printf("%s: %s (%d failure%s)\n", d.name.toRawUTF8(), failures ? "FAILED" : "passed", failures, failures == 1 ? "" : "s");
 return failures ? 1 : 0;
}
