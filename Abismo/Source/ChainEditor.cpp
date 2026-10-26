#include "ChainEditor.h"
#include <cmath>

using namespace inverso_abismo::chain;
namespace c = soft::colour;

namespace {
const juce::Colour eqCol{0xff3ccfe0}, gateCol{0xfff06bb0}, transCol{0xfff2a541}, tameCol{0xff9a86ff}, compCol{0xffff8a3d}, glueCol{0xffd23a3f}, satCol{0xffef7b3d}, widthCol{0xff52d39a}, limCol{0xff5aa9f5};

const char* const* shortNames() {
 static const char* names[] = {
  "INPUT",
  "EQ", "HIGH-PASS", "LOW-PASS", "LOW FREQ", "LOW GAIN", "B1 FREQ", "B1 GAIN", "B1 WIDTH", "B2 FREQ", "B2 GAIN", "B2 WIDTH", "HIGH FREQ", "HIGH GAIN", "WORKS ON",
  "GATE", "THRESHOLD", "RANGE", "HOLD", "RELEASE", "LISTEN ABOVE",
  "TRANSIENTS", "ATTACK", "SUSTAIN",
  "TAME", "FREQUENCY", "THRESHOLD", "RANGE", "WIDTH",
  "COMP", "TYPE", "THRESHOLD", "RATIO", "ATTACK", "RELEASE", "KNEE", "TIME", "MIX", "TRIM", "DETECTOR HPF",
  "GLUE", "TYPE", "THRESHOLD", "RATIO", "ATTACK", "RELEASE", "MIX", "TRIM",
  "SAT", "TYPE", "DRIVE", "TONE", "MIX",
  "WIDTH", "BASS MONO",
  "LIMITER", "PUSH", "CEILING", "AUTO REL", "RELEASE",
  "OUTPUT", "DITHER", "GLUE", "WARMTH", "TONE", "LOUDNESS"};
 static_assert(sizeof(names) / sizeof(names[0]) == P::Count, "one short name per parameter");
 return names;
}
void caption(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& left, const juce::String& right = {}) {
 g.setFont(soft::monoFont(10.f));
 g.setColour(c::screenMuted);
 g.drawText(left, r.withHeight(12.f), juce::Justification::topLeft);
 if (right.isNotEmpty()) g.drawText(right, r.withHeight(12.f), juce::Justification::topRight);
}
juce::String dbText(double v) { return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB"; }
juce::String lufsText(double v) { return v <= -69.9 ? juce::String("-inf") : juce::String(v, 1); }
} // namespace

// Momentary, short-term and integrated loudness and the highest peak; a click starts them over.
class InversoChainEditor::Loudness final : public juce::Component, public juce::SettableTooltipClient {
public:
 explicit Loudness(InversoChainProcessor& p) : processor(p) { setTooltip("Click to reset the integrated loudness and the peak"); }
 void update() {
  const auto& e = processor.engine;
  const float m = e.momentary.load(), s = e.shortTerm.load(), i = e.integrated.load(), k = e.peakDb.load();
  if (std::abs(m - mom) > 1e-4f || std::abs(s - shortT) > 1e-4f || std::abs(i - integ) > 1e-4f || std::abs(k - peak) > 1e-4f) { mom = m; shortT = s; integ = i; peak = k; repaint(); }
 }
 void paint(juce::Graphics& g) override {
  auto r = getLocalBounds().toFloat();
  g.setColour(c::screenTop);
  g.fillRoundedRectangle(r, 8.f);
  const char* labels[4] = {"MOMENTARY", "SHORT-TERM", "INTEGRATED", "PEAK"};
  const juce::String texts[4] = {lufsText(mom), lufsText(shortT), lufsText(integ), peak <= -119.f ? juce::String("-inf") : juce::String(peak, 1)};
  const char* units[4] = {"LUFS", "LUFS", "LUFS", "dBFS"};
  const float rowH = r.getHeight() / 4.f;
  for (int i = 0; i < 4; ++i) {
   auto row = r.withTrimmedTop(rowH * static_cast<float>(i)).withHeight(rowH).reduced(8.f, 2.f);
   g.setColour(c::screenMuted);
   g.setFont(soft::monoFont(9.f));
   g.drawText(labels[i], row, juce::Justification::topLeft);
   g.setColour(i == 3 && peak > -0.1f ? c::red : c::screenInk);
   g.setFont(soft::monoFont(15.f));
   g.drawText(texts[i] + " " + units[i], row, juce::Justification::bottomRight);
  }
 }
 void mouseUp(const juce::MouseEvent& e) override { if (contains(e.getPosition())) processor.engine.resetMeters(); }
private:
 InversoChainProcessor& processor;
 float mom = -70.f, shortT = -70.f, integ = -70.f, peak = -120.f;
};

InversoChainEditor::InversoChainEditor(InversoChainProcessor& p) : AudioProcessorEditor(&p), processor(p) {
 setLookAndFeel(&look);
 sat.prepare(48000.0);
 const auto strip = InversoChainProcessor::strip;
 const auto& lay = layout(strip);
 values = processor.currentValues();
 body.title = stripName(strip);
 body.subtitle = strip == Strip::Master ? juce::String::fromUTF8("MASTERING CHAIN   \xc2\xb7   EQ, COMPRESSION, SATURATION, LIMITER, LUFS")
                                         : juce::String::fromUTF8("CHANNEL STRIP   \xc2\xb7   DRUMS");
 addAndMakeVisible(body);
 presets.getCount = [this] { return processor.getNumPrograms(); };
 presets.getCurrent = [this] { return processor.getCurrentProgram(); };
 presets.getName = [this](int i) { return processor.getProgramName(i); };
 presets.select = [this](int i) { processor.selectProgram(i); };
 presets.enableUserPresets(processor.apvts, stripName(strip));
 body.addAndMakeVisible(presets);

 // A module numbers its parameters by the plugin's own table, not the global one.
 std::vector<const char*> names;
 for (int g : lay.global) names.push_back(shortNames()[g]);
 auto L = [&](int global) { return lay.local[static_cast<std::size_t>(global)]; };
 auto has = [&](int global) { return L(global) >= 0; };
 using Page = soft::Module::Page;
 auto local = [&](const std::vector<int>& ids) { std::vector<int> out; for (int g : ids) if (has(g)) out.push_back(L(g)); return out; };
 auto add = [&](Part part, const char* title, juce::Colour accent, std::vector<std::pair<const char*, std::vector<int>>> pages, std::vector<int> choices, int weight) {
  std::vector<Page> built;
  for (auto& [name, ids] : pages) built.push_back({name, local(ids)});
  Entry e;
  e.part = part;
  e.hasChoice = !choices.empty();
  e.module = std::make_unique<soft::Module>(processor.apvts, lay.specs.data(), names.data(), title, accent, std::move(built), 132, local(choices));
  e.weight = weight;
  body.addAndMakeVisible(*e.module);
  modules.push_back(std::move(e));
 };
 // the master's direct controls first: they work on top of the preset
 if (has(P::MGlue)) add(Part::Macro, "MASTER MACROS", c::ink, {{"Macros", {P::MGlue, P::MWarmth, P::MTone, P::MLoud}}}, {}, 5);
 // the order of the signal path
 if (has(P::GateOn)) add(Part::Gate, "GATE", gateCol, {{"Gate", {P::GateOn, P::GateThresh, P::GateRange, P::GateHold, P::GateRelease, P::GateFocus}}}, {}, 5);
 if (has(P::TrOn)) add(Part::Trans, "TRANSIENTS", transCol, {{"Transients", {P::TrOn, P::TrAttack, P::TrSustain}}}, {}, 3);
 add(Part::Eq, "EQ", eqCol, {{"Shelves", {P::EqOn, P::LsF, P::LsG, P::HsF, P::HsG}}, {"Bells", {P::B1F, P::B1G, P::B1Q, P::B2F, P::B2G, P::B2Q}}, {"Filters", {P::Hpf, P::Lpf}}}, {P::EqMs}, 6);
 if (has(P::TameOn)) add(Part::Tame, "TAME", tameCol, {{"Tame", {P::TameOn, P::TameFreq, P::TameThresh, P::TameRange, P::TameQ}}}, {}, 5);
 add(Part::Comp, "COMPRESSOR", compCol, {{"Compressor", {P::C1On, P::C1Thresh, P::C1Ratio, P::C1Attack, P::C1Release, P::C1Knee}}, {"More", {P::C1Time, P::C1Mix, P::C1Makeup, P::C1Sc}}}, {P::C1Type}, 6);
 if (has(P::C2On)) add(Part::Glue, "GLUE", glueCol, {{"Glue", {P::C2On, P::C2Thresh, P::C2Ratio, P::C2Attack, P::C2Release, P::C2Mix, P::C2Makeup}}}, {P::C2Type}, 6);
 add(Part::Sat, "SATURATION", satCol, {{"Saturation", {P::SatOn, P::SatDrive, P::SatTone, P::SatMix}}}, {P::SatKind}, 4);
 if (has(P::StereoWidth)) add(Part::Width, "STEREO", widthCol, {{"Stereo", {P::StereoWidth, P::BassMono}}}, {}, 3);
 if (has(P::LimOn)) add(Part::Limiter, "LIMITER", limCol, {{"Limiter", {P::LimOn, P::LimGain, P::LimCeil, P::LimAuto, P::LimRelease}}}, {P::DitherBits}, 5);
 // two rows of about the same width
 int total = 0; for (auto& e : modules) total += e.weight;
 int acc = 0;
 for (auto& e : modules) { e.row = acc * 2 < total - e.weight / 2 ? 0 : 1; acc += e.weight; }
 modules.back().row = 1;
 master = std::make_unique<soft::Master>(processor.apvts, lay.specs[static_cast<std::size_t>(L(P::OutGain))], &lay.specs[static_cast<std::size_t>(L(P::InGain))], juce::String("INPUT"), c::ink);
 body.addAndMakeVisible(*master);
 if (strip == Strip::Master) {
  auto l = std::make_unique<Loudness>(processor);
  loudness = l.get();
  master->setAccessory(std::move(l), 132);
 }
 setupScreens();
 setResizable(true, true);
 soft::sizeWindow(*this, stripName(strip), designW, designH, 0.6f);
 startTimerHz(30);
}

InversoChainEditor::~InversoChainEditor() { setLookAndFeel(nullptr); }

float InversoChainEditor::v(int global) const { return values[static_cast<std::size_t>(global)]; }

void InversoChainEditor::setupScreens() {
 for (auto& e : modules) {
  const bool choice = e.hasChoice;
  auto plot = [choice](juce::Rectangle<float> r) { auto a = r.reduced(4.f, 2.f); if (choice) a.removeFromBottom(34.f); return a; };
  auto grid = [](juce::Graphics& g, juce::Rectangle<float> p, int lines) {
   g.setColour(c::screenMuted.withAlpha(.18f));
   for (int i = 1; i < lines; ++i) g.drawHorizontalLine(juce::roundToInt(p.getY() + p.getHeight() * static_cast<float>(i) / static_cast<float>(lines)), p.getX(), p.getRight());
  };
  switch (e.part) {
   case Part::Macro:
    e.module->screen.draw = [this, plot](juce::Graphics& g, juce::Rectangle<float> r, int) {
     auto a = plot(r);
     caption(g, a, "ON TOP OF THE PRESET", "0 = AS THE PRESET");
     auto p = a.withTrimmedTop(16.f);
     const float st = processor.engine.shortTerm.load(), gr = processor.engine.gr1.load() + processor.engine.gr2.load() + processor.engine.grLim.load();
     g.setColour(c::screenInk);
     g.setFont(soft::monoFont(26.f));
     g.drawText(st <= -69.9f ? juce::String("-inf") : juce::String(st, 1), p.removeFromTop(p.getHeight() * .6f), juce::Justification::centred);
     g.setFont(soft::monoFont(10.f));
     g.setColour(c::screenMuted);
     g.drawText("LUFS SHORT-TERM    TOTAL REDUCTION " + juce::String(gr, 1) + " dB", p, juce::Justification::centred);
    };
    break;
   case Part::Eq:
    e.module->screen.draw = [this, plot, grid](juce::Graphics& g, juce::Rectangle<float> r, int) {
     auto a = plot(r);
     EqControls k;
     k.on = v(P::EqOn) > .5f; k.hpf = v(P::Hpf); k.lpf = v(P::Lpf); k.lsF = v(P::LsF); k.lsG = v(P::LsG); k.b1F = v(P::B1F); k.b1G = v(P::B1G); k.b1Q = v(P::B1Q);
     k.b2F = v(P::B2F); k.b2G = v(P::B2G); k.b2Q = v(P::B2Q); k.hsF = v(P::HsF); k.hsG = v(P::HsG);
     Eq eq; eq.prepare(48000.0); eq.setControls(k);
     caption(g, a, k.on ? "FREQUENCY RESPONSE" : "EQ OFF", "+/-15 dB");
     auto p = a.withTrimmedTop(16.f).reduced(0.f, 4.f);
     grid(g, p, 6);
     juce::Path path;
     for (int i = 0; i <= 240; ++i) {
      const double t = i / 240.0, f = 20.0 * std::pow(1000.0, t);
      const float db = k.on ? static_cast<float>(juce::jlimit(-15.0, 15.0, eq.responseDb(f))) : 0.f;
      const float x = p.getX() + static_cast<float>(t) * p.getWidth(), y = p.getCentreY() - db / 15.f * p.getHeight() * .5f;
      if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
     }
     soft::glowStroke(g, path, eqCol.withAlpha(k.on ? 1.f : .35f));
    };
    break;
   case Part::Gate:
    e.module->screen.draw = [this, plot, grid](juce::Graphics& g, juce::Rectangle<float> r, int) {
     auto a = plot(r);
     const bool on = v(P::GateOn) > .5f;
     caption(g, a, on ? juce::String("OPENS AT ") + abismo_ui::formatValue(allSpecs()[P::GateThresh], v(P::GateThresh)) : juce::String("GATE OFF"), on ? juce::String(processor.engine.gateNow.load() > .5f ? "OPEN" : "SHUT") : juce::String());
     auto p = a.withTrimmedTop(16.f).reduced(0.f, 4.f);
     grid(g, p, 4);
     juce::Path path;
     const int n = static_cast<int>(gateHistory.size());
     for (int i = 0; i < n; ++i) {
      const float x = p.getX() + p.getWidth() * static_cast<float>(i) / static_cast<float>(n - 1), y = p.getBottom() - gateHistory[static_cast<std::size_t>(i)] * p.getHeight();
      if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
     }
     soft::glowStroke(g, path, gateCol.withAlpha(on ? 1.f : .3f));
    };
    break;
   case Part::Trans:
    e.module->screen.draw = [this, plot](juce::Graphics& g, juce::Rectangle<float> r, int) {
     auto a = plot(r);
     const bool on = v(P::TrOn) > .5f;
     const float atk = v(P::TrAttack) * .01f, sus = v(P::TrSustain) * .01f;
     caption(g, a, on ? "ATTACK " + dbText(atk * 12.0) : juce::String("TRANSIENTS OFF"), on ? "SUSTAIN " + dbText(sus * 12.0) : juce::String());
     auto p = a.withTrimmedTop(16.f).reduced(0.f, 4.f);
     juce::Path before, after;
     for (int i = 0; i <= 200; ++i) {
      const float t = static_cast<float>(i) / 200.f;
      const float env = std::exp(-t * 5.f), onset = std::exp(-t * 40.f);
      const float mod = on ? std::pow(10.f, (atk * 12.f * onset + sus * 12.f * (1.f - onset)) / 20.f) : 1.f;
      const float x = p.getX() + t * p.getWidth();
      const float y0 = p.getBottom() - juce::jlimit(0.f, 1.f, env) * p.getHeight() * .75f, y1 = p.getBottom() - juce::jlimit(0.f, 1.f, env * mod * .85f) * p.getHeight() * .75f;
      if (i == 0) { before.startNewSubPath(x, y0); after.startNewSubPath(x, y1); } else { before.lineTo(x, y0); after.lineTo(x, y1); }
     }
     g.setColour(c::screenMuted.withAlpha(.5f));
     g.strokePath(before, juce::PathStrokeType(1.f));
     soft::glowStroke(g, after, transCol.withAlpha(on ? 1.f : .35f));
    };
    break;
   case Part::Tame:
    e.module->screen.draw = [this, plot, grid](juce::Graphics& g, juce::Rectangle<float> r, int) {
     auto a = plot(r);
     const bool on = v(P::TameOn) > .5f;
     const float gr = processor.engine.grTm.load();
     caption(g, a, on ? abismo_ui::formatValue(allSpecs()[P::TameFreq], v(P::TameFreq)) : juce::String("TAME OFF"), on ? dbText(-gr) : juce::String());
     auto p = a.withTrimmedTop(16.f).reduced(0.f, 4.f);
     grid(g, p, 4);
     juce::Path path;
     const double f0 = v(P::TameFreq), q = v(P::TameQ);
     for (int i = 0; i <= 200; ++i) {
      const double t = i / 200.0, f = 20.0 * std::pow(1000.0, t), o = std::log2(f / f0) * q;
      const float dip = on ? static_cast<float>(gr * std::exp(-o * o)) : 0.f;
      const float x = p.getX() + static_cast<float>(t) * p.getWidth(), y = p.getY() + p.getHeight() * .3f + dip / 12.f * p.getHeight() * .6f;
      if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
     }
     soft::glowStroke(g, path, tameCol.withAlpha(on ? 1.f : .35f));
    };
    break;
   case Part::Comp:
   case Part::Glue: {
    const bool glue = e.part == Part::Glue;
    e.module->screen.draw = [this, plot, grid, glue](juce::Graphics& g, juce::Rectangle<float> r, int) {
     auto a = plot(r);
     const bool on = v(glue ? P::C2On : P::C1On) > .5f;
     CompControls k;
     k.on = true; k.type = static_cast<CompType>(juce::jlimit(0, 3, juce::roundToInt(v(glue ? P::C2Type : P::C1Type))));
     k.thresh = v(glue ? P::C2Thresh : P::C1Thresh); k.ratio = v(glue ? P::C2Ratio : P::C1Ratio); k.knee = glue ? 6.f : v(P::C1Knee);
     Compressor comp; comp.prepare(48000.0); comp.setControls(k);
     const float live = glue ? processor.engine.gr2.load() : processor.engine.gr1.load();
     caption(g, a, on ? juce::String("INPUT -> OUTPUT") : juce::String("OFF"), on ? "GR " + juce::String(live, 1) + " dB" : juce::String());
     auto p = a.withTrimmedTop(16.f).reduced(2.f, 4.f);
     auto meter = p.removeFromRight(10.f);
     p.removeFromRight(8.f);
     grid(g, p, 4);
     g.setColour(c::screenMuted.withAlpha(.4f));
     g.drawLine(p.getX(), p.getBottom(), p.getRight(), p.getY(), 1.f);
     juce::Path path;
     for (int i = 0; i <= 120; ++i) {
      const double in = -60.0 + 60.0 * i / 120.0, out = in - comp.staticGr(in);
      const float x = p.getX() + static_cast<float>(i) / 120.f * p.getWidth(), y = p.getBottom() - static_cast<float>((out + 60.0) / 60.0) * p.getHeight();
      if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
     }
     soft::glowStroke(g, path, (glue ? glueCol : compCol).withAlpha(on ? 1.f : .35f));
     g.setColour(c::screenMuted.withAlpha(.25f));
     g.fillRoundedRectangle(meter, 3.f);
     g.setColour((glue ? glueCol : compCol).withAlpha(on ? 1.f : .35f));
     const float fill = juce::jlimit(0.f, 1.f, live / 20.f) * meter.getHeight();
     g.fillRoundedRectangle(meter.withHeight(fill), 3.f);
    };
    break;
   }
   case Part::Sat:
    e.module->screen.draw = [this, plot, grid](juce::Graphics& g, juce::Rectangle<float> r, int) {
     auto a = plot(r);
     const bool on = v(P::SatOn) > .5f;
     SatControls k;
     k.on = true; k.type = static_cast<SatType>(juce::jlimit(0, 2, juce::roundToInt(v(P::SatKind)))); k.drive = v(P::SatDrive); k.tone = v(P::SatTone); k.mix = v(P::SatMix);
     sat.setControls(k);   // prepared once: its loudness table takes a while to build
     caption(g, a, on ? juce::String("WAVEFORM, -3 dBFS SINE") : juce::String("SATURATION OFF"), on ? abismo_ui::formatValue(allSpecs()[P::SatDrive], v(P::SatDrive)) : juce::String());
     auto p = a.withTrimmedTop(16.f).reduced(0.f, 4.f);
     grid(g, p, 4);
     juce::Path dry, wet;
     const int per = 96, cycles = 6;
     std::vector<double> out;
     for (int i = 0; i < per * cycles; ++i) out.push_back(sat.process(0, .7 * std::sin(2.0 * pi * i / per)));
     for (int i = 0; i < per; ++i) {
      const float x = p.getX() + static_cast<float>(i) / static_cast<float>(per - 1) * p.getWidth();
      const float yd = p.getCentreY() - static_cast<float>(.7 * std::sin(2.0 * pi * (per * (cycles - 1) + i) / per)) * p.getHeight() * .5f;
      const float yw = p.getCentreY() - static_cast<float>(juce::jlimit(-1.2, 1.2, out[static_cast<std::size_t>(per * (cycles - 1) + i)])) * p.getHeight() * .5f;
      if (i == 0) { dry.startNewSubPath(x, yd); wet.startNewSubPath(x, yw); } else { dry.lineTo(x, yd); wet.lineTo(x, yw); }
     }
     g.setColour(c::screenMuted.withAlpha(.5f));
     g.strokePath(dry, juce::PathStrokeType(1.f));
     soft::glowStroke(g, wet, satCol.withAlpha(on ? 1.f : .35f));
    };
    break;
   case Part::Width:
    e.module->screen.draw = [this, plot](juce::Graphics& g, juce::Rectangle<float> r, int) {
     auto a = plot(r);
     const float w = v(P::StereoWidth) * .01f, bm = v(P::BassMono);
     caption(g, a, "WIDTH " + juce::String(juce::roundToInt(v(P::StereoWidth))) + "%", bm > 20.5f ? "MONO BELOW " + abismo_ui::formatValue(allSpecs()[P::BassMono], bm) : juce::String("BASS STEREO"));
     auto p = a.withTrimmedTop(16.f).reduced(0.f, 4.f);
     const auto centre = p.getCentre();
     const float rx = p.getWidth() * .5f * juce::jlimit(.04f, 1.f, w * .5f), ry = p.getHeight() * .46f;
     g.setColour(c::screenMuted.withAlpha(.25f));
     g.drawLine(p.getX(), centre.y, p.getRight(), centre.y, 1.f);
     g.drawLine(centre.x, p.getY(), centre.x, p.getBottom(), 1.f);
     juce::Path ell; ell.addEllipse(centre.x - rx, centre.y - ry, rx * 2.f, ry * 2.f);
     soft::glowStroke(g, ell, widthCol);
    };
    break;
   case Part::Limiter:
    e.module->screen.draw = [this, plot, grid](juce::Graphics& g, juce::Rectangle<float> r, int) {
     auto a = plot(r);
     const bool on = v(P::LimOn) > .5f;
     caption(g, a, on ? "CEILING " + dbText(v(P::LimCeil)) : juce::String("LIMITER OFF"), on ? "GR " + juce::String(processor.engine.grLim.load(), 1) + " dB" : juce::String());
     auto p = a.withTrimmedTop(16.f).reduced(0.f, 4.f);
     grid(g, p, 4);
     juce::Path path;
     const int n = static_cast<int>(limiterHistory.size());
     for (int i = 0; i < n; ++i) {
      const float x = p.getX() + p.getWidth() * static_cast<float>(i) / static_cast<float>(n - 1), y = p.getY() + juce::jlimit(0.f, 1.f, limiterHistory[static_cast<std::size_t>(i)] / 12.f) * p.getHeight();
      if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
     }
     soft::glowStroke(g, path, limCol.withAlpha(on ? 1.f : .3f));
    };
    break;
  }
 }
}

void InversoChainEditor::timerCallback() {
 presets.refresh();
 values = processor.currentValues();
 std::rotate(gateHistory.begin(), gateHistory.begin() + 1, gateHistory.end());
 gateHistory.back() = processor.engine.gateNow.load();
 std::rotate(limiterHistory.begin(), limiterHistory.begin() + 1, limiterHistory.end());
 limiterHistory.back() = processor.engine.grLim.load();
 for (auto& e : modules) e.module->screen.repaint();
 if (loudness != nullptr) loudness->update();
 master->setReadout(processor.outputPeak.load(), processor.engine.latency() > 0 ? juce::String("LATENCY ") + juce::String(processor.engine.latency()) + " SAMPLES" : juce::String("ZERO LATENCY"));
}

void InversoChainEditor::resized() {
 body.setBounds(0, 0, designW, designH);
 body.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / designW));
 soft::rememberWindow(*this, stripName(InversoChainProcessor::strip), designW);
 presets.setBounds(designW / 2 - 200, 14, 400, 72);
 const int x0 = 10, y0 = 88, masterW = 200, gap = 0;
 const int rowsH = (designH - y0 - 12) / 2, usable = designW - x0 - masterW - 20;
 for (int row = 0; row < 2; ++row) {
  int sum = 0; for (auto& e : modules) if (e.row == row) sum += e.weight;
  int x = x0;
  for (auto& e : modules) {
   if (e.row != row) continue;
   const int w = juce::roundToInt(static_cast<float>(usable) * static_cast<float>(e.weight) / static_cast<float>(std::max(1, sum)));
   e.module->setBounds(x, y0 + row * rowsH, w - gap, rowsH);
   x += w;
  }
 }
 master->setBounds(designW - masterW - 10, y0, masterW, designH - y0 - 12);
}
