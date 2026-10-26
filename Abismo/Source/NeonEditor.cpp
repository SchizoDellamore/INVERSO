#include "NeonEditor.h"
#include "DSP/NeonEngine.h"
#include <cmath>
#include <complex>

using namespace inverso_abismo::neon;
namespace c = soft::colour;

namespace {
constexpr int designW = 1400, designH = 880;
const juce::Colour neonPink{0xffff4fa3}, neonViolet{0xff9a6bff}, neonCyan{0xff2fd6e8}, sunset{0xffff8a3d};

const char* const* shortNames() {
 static const char* names[] = {
  "WAVE", "OCTAVE", "WIDTH", "LEVEL",
  "WAVE", "OCTAVE", "SEMI", "FINE", "SYNC", "LEVEL",
  "SUB", "NOISE", "CROSS MOD", "PWM",
  "LEVEL", "RATIO", "BRIGHT", "DECAY", "VELOCITY",
  "TYPE", "CUTOFF", "RESONANCE", "ENVELOPE", "KEYTRACK", "VELOCITY", "DRIVE",
  "ATTACK", "DECAY", "SUSTAIN", "RELEASE",
  "ATTACK", "DECAY", "SUSTAIN", "RELEASE",
  "RATE", "SHAPE", "TIMING", "FADE IN", "VIBRATO", "FILTER", "LEVEL",
  "MODE", "UNISON", "DETUNE", "SPREAD", "GLIDE", "VELOCITY", "BEND", "OCTAVE", "DRIFT",
  "ARP", "RATE", "MODE", "OCTAVES", "GATE", "SWING", "LATCH",
  "CHORUS", "MIX", "MIX", "TIMING", "TIME", "REPEATS", "PING-PONG", "TONE",
  "MIX", "TYPE", "SIZE", "DECAY", "DARKNESS",
  "WIDTH", "OUTPUT", "AT KNOB"};
 static_assert(sizeof(names) / sizeof(names[0]) == P::Count, "one short name per parameter");
 return names;
}

juce::Path curve(juce::Rectangle<float> r, int points, const std::function<float(float)>& f) {
 juce::Path p;
 for (int i = 0; i <= points; ++i) {
  const float t = static_cast<float>(i) / static_cast<float>(points);
  const float y = juce::jlimit(-1.2f, 1.2f, f(t));
  const juce::Point<float> pt(r.getX() + t * r.getWidth(), r.getCentreY() - y * r.getHeight() * .5f);
  if (i == 0) p.startNewSubPath(pt); else p.lineTo(pt);
 }
 return p;
}
void caption(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& left, const juce::String& right = {}) {
 g.setFont(soft::monoFont(10.f));
 g.setColour(c::screenMuted);
 g.drawText(left, r.withHeight(12.f), juce::Justification::topLeft);
 if (right.isNotEmpty()) g.drawText(right, r.withHeight(12.f), juce::Justification::topRight);
}
juce::Path adsr(juce::Rectangle<float> r, float a, float d, float s, float rel) {
 auto w = [](float ms) { return std::log10(1.f + ms / 4.f); };
 const float total = w(a) + w(d) + 1.2f + w(rel);
 const float k = r.getWidth() / total;
 const float x1 = r.getX() + w(a) * k, x2 = x1 + w(d) * k, x3 = x2 + 1.2f * k, x4 = x3 + w(rel) * k;
 const float top = r.getY(), bottom = r.getBottom(), sy = bottom - s * r.getHeight();
 juce::Path p;
 p.startNewSubPath(r.getX(), bottom);
 p.quadraticTo((r.getX() + x1) * .5f, top + (bottom - top) * .15f, x1, top);
 p.quadraticTo(x1 + (x2 - x1) * .25f, sy, x2, sy);
 p.lineTo(x3, sy);
 p.quadraticTo(x3 + (x4 - x3) * .2f, bottom, x4, bottom);
 return p;
}
float waveAt(int wave, float t, float pw) {
 switch (wave) {
  case 1: return t < pw ? .8f : -.8f;
  case 2: return t < .5f ? 4.f * t - 1.f : 3.f - 4.f * t;
  case 3: return std::sin(t * 6.2831853f);
  default: return 1.f - 2.f * t;
 }
}
} // namespace

float InversoNeonEditor::value(int param) const {
 return processor.apvts.getRawParameterValue(specs()[static_cast<std::size_t>(param)].id)->load();
}

InversoNeonEditor::InversoNeonEditor(InversoNeonProcessor& p) : AudioProcessorEditor(&p), processor(p) {
 setLookAndFeel(&look);
 body.title = "NEON";
 body.subtitle = juce::String::fromUTF8("A SYNTHWAVE POLYSYNTH   \xc2\xb7   MOD WHEEL: BRIGHTER + VIBRATO");
 addAndMakeVisible(body);
 presets.getCount = [this] { return processor.getNumPrograms(); };
 presets.getCurrent = [this] { return processor.getCurrentProgram(); };
 presets.getName = [this](int i) { return processor.getProgramName(i); };
 presets.select = [this](int i) { processor.selectProgram(i); };
 presets.enableUserPresets(processor.apvts, "NEON");
 body.addAndMakeVisible(presets);
 using Page = soft::Module::Page;
 auto add = [&](juce::String title, juce::Colour accent, std::vector<Page> pages, int screen, std::vector<int> choices) {
  modules.push_back(std::make_unique<soft::Module>(processor.apvts, specs().data(), shortNames(), title, accent, std::move(pages), screen, std::move(choices)));
  body.addAndMakeVisible(*modules.back());
 };
 add("OSC 1", neonPink, {{"Osc 1", {P::Osc1Octave, P::Osc1PW, P::PwmDepth, P::Osc1Level}, {P::Osc1Level}}}, 90, {P::Osc1Wave});
 add("OSC 2", neonViolet, {{"Osc 2", {P::Osc2Octave, P::Osc2Semi, P::Osc2Fine, P::Osc2Level, P::CrossMod, P::Osc2Sync}, {P::Osc2Level}}}, 90, {P::Osc2Wave});
 add("MIX + FM", neonCyan, {{"Mix", {P::SubLevel, P::NoiseLevel, P::Drift}}, {"FM", {P::FmLevel, P::FmIndex, P::FmDecay, P::FmVelocity}, {P::FmLevel}}}, 90, {P::FmRatio});
 add("FILTER", sunset, {{"Filter", {P::Cutoff, P::Resonance, P::FilterEnv, P::Keytrack, P::FilterVel, P::FilterDrive}, {P::Cutoff}}}, 90, {P::FilterType});
 add("ENVELOPES", neonPink.interpolatedWith(sunset, .5f), {{"Amp", {P::Attack, P::Decay, P::Sustain, P::Release}, {}, true},
                                                         {"Filter", {P::FAttack, P::FDecay, P::FSustain, P::FRelease}, {}, true}}, 90, {});
 add("LFO", neonCyan.interpolatedWith(neonViolet, .5f), {{"LFO", {P::LfoRate, P::LfoDelay, P::LfoPitch, P::LfoFilter, P::LfoAmp}, {P::LfoRate}}}, 90,
     {P::LfoShape, P::LfoSync});
 add("VOICE", neonViolet, {{"Voice", {P::Unison, P::Detune, P::Spread, P::Glide}, {P::Unison}}, {"Play", {P::VelSens, P::BendRange, P::Octave}}}, 90, {P::VoiceMode});
 add("ARP", neonPink, {{"Arp", {P::ArpOctaves, P::Gate, P::Swing, P::Latch}}}, 90, {P::ArpRate, P::ArpMode});
 add("FX", neonCyan, {{"Chorus", {P::ChorusMix}, {P::ChorusMix}},
                      {"Delay", {P::DelayMix, P::DelayTime, P::DelayFeedback, P::DelayTone, P::DelayPingPong}, {P::DelayMix}},
                      {"Reverb", {P::RevMix, P::RevSize, P::RevDecay, P::RevDamp}, {P::RevMix}}}, 90, {P::ChorusMode, P::DelaySync, P::RevType});
 master = std::make_unique<soft::Master>(processor.apvts, specs()[P::Output], &specs()[P::Width], juce::String("WIDTH"), c::ink);
 body.addAndMakeVisible(*master);
 {
  auto& b = arpOn;
  b.setClickingTogglesState(true);
  b.setTooltip("Arpeggiator on or off: hold a chord and it plays it on the host's tempo");
  b.setColour(juce::TextButton::textColourOnId, neonPink.darker(.3f));
  b.setColour(juce::TextButton::textColourOffId, c::muted);
  b.onStateChange = [&b] { const auto text = b.getToggleState() ? "ON" : "OFF"; if (b.getButtonText() != text) b.setButtonText(text); };
  arpAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "ARP", b);
  b.onStateChange();
  modules[7]->addHeaderComponent(b, 44);
 }
 auto& m = modules;
 auto screenArea = [](juce::Rectangle<float> r) { return r.withTrimmedBottom(34.f); };
 m[0]->screen.draw = [this, screenArea](juce::Graphics& g, juce::Rectangle<float> r, int) {
  auto a = screenArea(r);
  const int wave = juce::roundToInt(value(P::Osc1Wave));
  caption(g, a, abismo_ui::formatValue(specs()[P::Osc1Octave], value(P::Osc1Octave)), value(P::SubLevel) > 0.f ? juce::String("+ SUB") : juce::String());
  const float pw = value(P::Osc1PW) * .01f;
  soft::glowStroke(g, curve(a.withTrimmedTop(14.f), 200, [&](float t) { return .8f * waveAt(wave, std::fmod(t * 2.f, 1.f), pw); }), neonPink);
 };
 m[1]->screen.draw = [this, screenArea](juce::Graphics& g, juce::Rectangle<float> r, int) {
  auto a = screenArea(r);
  const int wave = juce::roundToInt(value(P::Osc2Wave));
  const bool sync = value(P::Osc2Sync) >= .5f;
  const float ratio = std::exp2(value(P::Osc2Octave) + value(P::Osc2Semi) / 12.f);
  caption(g, a, abismo_ui::formatValue(specs()[P::Osc2Semi], value(P::Osc2Semi)) + "  " + abismo_ui::formatValue(specs()[P::Osc2Fine], value(P::Osc2Fine)),
          sync ? juce::String("SYNC") : juce::String());
  const auto colour = neonViolet.withAlpha(value(P::Osc2Level) > 0.f ? 1.f : .35f);
  soft::glowStroke(g, curve(a.withTrimmedTop(14.f), 240, [&](float t) {
   const float lead = std::fmod(t * 2.f, 1.f);
   const float ph = sync ? std::fmod(lead * ratio, 1.f) : std::fmod(t * 2.f * ratio, 1.f);
   return .8f * waveAt(wave, ph, .5f);
  }), colour);
 };
 m[2]->screen.draw = [this, screenArea](juce::Graphics& g, juce::Rectangle<float> r, int page) {
  auto a = screenArea(r);
  if (page == 0) {
   caption(g, a, "SUB " + juce::String(juce::roundToInt(value(P::SubLevel))) + "%  NOISE " + juce::String(juce::roundToInt(value(P::NoiseLevel))) + "%",
           "DRIFT " + juce::String(juce::roundToInt(value(P::Drift))) + "%");
   const float sub = value(P::SubLevel) * .01f, noise = value(P::NoiseLevel) * .01f;
   juce::Random rnd(7);
   soft::glowStroke(g, curve(a.withTrimmedTop(14.f), 240, [&](float t) {
    return .7f * (sub * (std::fmod(t * 2.f, 1.f) < .5f ? 1.f : -1.f) * .7f + noise * (rnd.nextFloat() * 2.f - 1.f) * .6f);
   }), neonCyan.withAlpha(sub + noise > 0.f ? 1.f : .35f));
   return;
  }
  static const float ratios[10] = {.5f, 1.f, 2.f, 3.f, 4.f, 5.f, 7.f, 9.f, 11.f, 14.f};
  const float ratio = ratios[juce::jlimit(0, 9, juce::roundToInt(value(P::FmRatio)))];
  const float index = value(P::FmIndex) * .01f * 7.f;
  caption(g, a, "FM " + juce::String(juce::roundToInt(value(P::FmLevel))) + "%", "BRIGHT " + juce::String(juce::roundToInt(value(P::FmIndex))) + "%");
  soft::glowStroke(g, curve(a.withTrimmedTop(14.f), 300, [&](float t) {
   const float ph = t * 2.f;
   return .8f * std::sin(6.2831853f * ph + index * std::sin(6.2831853f * ph * ratio));
  }), neonCyan.withAlpha(value(P::FmLevel) > 0.f ? 1.f : .35f));
 };
 m[3]->screen.draw = [this, screenArea](juce::Graphics& g, juce::Rectangle<float> r, int) {
  auto a = screenArea(r);
  const float fc = value(P::Cutoff), res = value(P::Resonance) * .01f;
  const int type = juce::roundToInt(value(P::FilterType));
  caption(g, a, abismo_ui::formatValue(specs()[P::Cutoff], fc), "ENV " + juce::String(juce::roundToInt(value(P::FilterEnv))) + "%");
  auto plot = a.withTrimmedTop(14.f);
  juce::Path path;
  for (int i = 0; i <= 200; ++i) {
   const float t = static_cast<float>(i) / 200.f, f = 20.f * std::pow(1000.f, t);
   const float x = f / fc;
   const float q = .5f + res * (type == 0 ? 6.f : 4.f);
   std::complex<float> s(0.f, x);
   std::complex<float> h;
   const auto den = s * s + s / q + 1.f;
   if (type == 2) h = (s / q) / den; else if (type == 3) h = (s * s) / den; else h = 1.f / den;
   if (type == 0) h = h * h; // 4 poles
   const float db = 20.f * std::log10(std::abs(h) + 1e-6f);
   const float y = plot.getY() + juce::jlimit(0.f, 1.f, (12.f - db) / 60.f) * plot.getHeight();
   if (i == 0) path.startNewSubPath(plot.getX() + t * plot.getWidth(), y); else path.lineTo(plot.getX() + t * plot.getWidth(), y);
  }
  soft::glowStroke(g, path, sunset);
 };
 m[4]->screen.draw = [this, screenArea](juce::Graphics& g, juce::Rectangle<float> r, int page) {
  auto a = r.reduced(4.f, 2.f);
  const int b = page == 0 ? P::Attack : P::FAttack;
  caption(g, a, page == 0 ? juce::String("LEVEL") : juce::String("FILTER"), page == 1 ? "AMOUNT " + juce::String(juce::roundToInt(value(P::FilterEnv))) + "%" : juce::String());
  soft::glowStroke(g, adsr(a.withTrimmedTop(16.f).reduced(2.f, 4.f), value(b), value(b + 1), value(b + 2) * .01f, value(b + 3)), neonPink.interpolatedWith(sunset, .5f));
  juce::ignoreUnused(screenArea);
 };
 m[5]->screen.draw = [this, screenArea](juce::Graphics& g, juce::Rectangle<float> r, int) {
  auto a = screenArea(r);
  const int shape = juce::roundToInt(value(P::LfoShape)), sync = juce::roundToInt(value(P::LfoSync));
  static const char* syncs[7] = {"", "1 BAR", "1/2", "1/4", "1/8", "1/16", "1/8T"};
  caption(g, a, sync > 0 ? juce::String(syncs[sync]) : abismo_ui::formatValue(specs()[P::LfoRate], value(P::LfoRate)),
          "FADE " + abismo_ui::formatValue(specs()[P::LfoDelay], value(P::LfoDelay)));
  juce::Random rnd(3);
  float held = 0.f;
  int last = -1;
  soft::glowStroke(g, curve(a.withTrimmedTop(14.f), 240, [&](float t) {
   const float ph = std::fmod(t * 3.f, 1.f);
   const int cycle = static_cast<int>(t * 3.f);
   if (cycle != last) { held = rnd.nextFloat() * 1.6f - .8f; last = cycle; }
   switch (shape) {
    case 1: return .8f * (ph < .5f ? 4.f * ph - 1.f : 3.f - 4.f * ph);
    case 2: return ph < .5f ? .8f : -.8f;
    case 3: return held;
    default: return .8f * std::sin(ph * 6.2831853f);
   }
  }), neonCyan.interpolatedWith(neonViolet, .5f));
 };
 m[6]->screen.draw = [this, screenArea](juce::Graphics& g, juce::Rectangle<float> r, int) {
  auto a = screenArea(r);
  const int uni = juce::roundToInt(value(P::Unison));
  const float detune = value(P::Detune) * .01f, spread = value(P::Spread) * .01f;
  caption(g, a, uni == 1 ? juce::String("ONE VOICE A NOTE") : juce::String(uni) + " VOICES A NOTE", processor.voicesSounding.load() > 0 ? juce::String(processor.voicesSounding.load()) + " SOUNDING" : juce::String());
  auto plot = a.withTrimmedTop(16.f).reduced(10.f, 4.f);
  for (int u = 0; u < uni; ++u) {
   const float pos = uni > 1 ? static_cast<float>(u) / static_cast<float>(uni - 1) * 2.f - 1.f : 0.f;
   const float x = plot.getCentreX() + pos * spread * plot.getWidth() * .45f;
   juce::Path path;
   path.startNewSubPath(plot.getCentreX(), plot.getBottom());
   path.lineTo(x, plot.getY() + (1.f - std::abs(pos) * detune) * 6.f);
   soft::glowStroke(g, path, neonViolet, 1.4f);
  }
 };
 m[7]->screen.draw = [this, screenArea](juce::Graphics& g, juce::Rectangle<float> r, int) {
  auto a = screenArea(r);
  const bool on = value(P::Arp) >= .5f;
  const int step = processor.arpStep.load();
  caption(g, a, on ? juce::String("HOLD A CHORD") : juce::String("ARPEGGIATOR OFF"), on && value(P::Latch) >= .5f ? juce::String("LATCHED") : juce::String());
  auto row = a.withTrimmedTop(18.f).reduced(6.f, 8.f);
  const float w = row.getWidth() / 16.f;
  for (int k = 0; k < 16; ++k) {
   const auto cell = juce::Rectangle<float>(row.getX() + k * w + 2.f, row.getCentreY() - 6.f, w - 4.f, 12.f);
   g.setColour(on && k == step ? neonPink : (on ? neonPink.withAlpha(.25f) : c::screenMuted.withAlpha(.2f)));
   g.fillRoundedRectangle(cell, 2.f);
  }
 };
 m[8]->screen.draw = [this, screenArea](juce::Graphics& g, juce::Rectangle<float> r, int page) {
  auto a = screenArea(r);
  if (page == 0) {
   static const char* names[5] = {"OFF", "I", "II", "I + II", "DIMENSION"};
   const int mode = juce::roundToInt(value(P::ChorusMode));
   caption(g, a, "CHORUS " + juce::String(names[juce::jlimit(0, 4, mode)]), "MIX " + juce::String(juce::roundToInt(value(P::ChorusMix))) + "%");
   const float rate = mode == 3 ? 6.f : mode == 2 ? 2.f : 1.2f;
   soft::glowStroke(g, curve(a.withTrimmedTop(14.f), 200, [&](float t) { return .5f * std::sin(t * 6.2831853f * rate); }), neonCyan.withAlpha(mode > 0 ? 1.f : .35f));
   soft::glowStroke(g, curve(a.withTrimmedTop(14.f), 200, [&](float t) { return -.5f * std::sin(t * 6.2831853f * rate); }), neonPink.withAlpha(mode > 0 ? 1.f : .35f));
   return;
  }
  if (page == 1) {
   const float mix = value(P::DelayMix) * .01f, fb = value(P::DelayFeedback) * .01f;
   caption(g, a, "DELAY " + juce::String(juce::roundToInt(mix * 100.f)) + "%", value(P::DelayPingPong) >= .5f ? juce::String("PING-PONG") : juce::String());
   auto plot = a.withTrimmedTop(16.f);
   float level = 1.f;
   for (int k = 0; k < 8; ++k) {
    const float x = plot.getX() + 10.f + k * (plot.getWidth() - 20.f) / 8.f;
    const float h = plot.getHeight() * .8f * level * (k == 0 ? 1.f : mix);
    const bool left = value(P::DelayPingPong) < .5f || k % 2 == 0;
    g.setColour((left ? neonCyan : neonPink).withAlpha(mix > 0.f || k == 0 ? .9f : .3f));
    g.fillRect(x, plot.getBottom() - h, 4.f, h);
    if (k > 0) level *= fb;
   }
   return;
  }
  const float decay = value(P::RevDecay);
  caption(g, a, juce::roundToInt(value(P::RevType)) == 0 ? juce::String("PLATE") : juce::String("HALL"), abismo_ui::formatValue(specs()[P::RevDecay], decay));
  soft::glowStroke(g, curve(a.withTrimmedTop(14.f), 200, [&](float t) { return 1.6f * std::exp(-t * 6.f / std::max(.3f, decay)) - .8f; }),
                   neonViolet.withAlpha(value(P::RevMix) > 0.f ? 1.f : .35f));
 };
 setResizable(true, true);
 soft::sizeWindow(*this,"NEON",designW,designH,0.5f);
 startTimerHz(30);
}

InversoNeonEditor::~InversoNeonEditor() { setLookAndFeel(nullptr); }

void InversoNeonEditor::timerCallback() {
 presets.refresh();
 for (auto& m : modules) m->screen.repaint();
 master->setReadout(processor.outputPeak.load(), "VOICES " + juce::String(processor.voicesSounding.load()) + " / 32");
}

void InversoNeonEditor::resized() {
 body.setBounds(0, 0, designW, designH);
 body.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / designW));
 soft::rememberWindow(*this,"NEON",designW);
 presets.setBounds(designW / 2 - 230, 14, 460, 72);
 const int x0 = 10, y0 = 88, mainW = 1150, row = (designH - y0 - 12) / 3;
 auto& m = modules;
 auto place = [&](int i, int x, int y, int w) { m[static_cast<std::size_t>(i)]->setBounds(x0 + x, y0 + y * row, w, row); };
 place(0, 0, 0, 340); place(1, 340, 0, 420); place(2, 760, 0, mainW - 760);
 place(3, 0, 1, 450); place(4, 450, 1, 320); place(5, 770, 1, mainW - 770);
 place(6, 0, 2, 380); place(7, 380, 2, 330); place(8, 710, 2, mainW - 710);
 master->setBounds(x0 + mainW, y0, designW - (x0 + mainW) - 10, row * 3);
}
