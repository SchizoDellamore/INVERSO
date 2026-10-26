#include "HaloEditor.h"
#include <cmath>

using namespace inverso_abismo::halo;
namespace c = soft::colour;

namespace {
constexpr int designW = 1040, designH = 600;
const juce::Colour neonPink{0xffff4fa3}, neonViolet{0xff9a6bff}, neonCyan{0xff2fd6e8};
const char* const* shortNames() {
 static const char* names[] = {"REVERB", "MIX", "PRE-DELAY", "SIZE", "DECAY", "DARKNESS", "LOW CUT", "HIGH CUT",
                               "THRESHOLD", "HOLD", "RELEASE", "WIDTH", "OUTPUT"};
 static_assert(sizeof(names) / sizeof(names[0]) == P::Count, "one short name per parameter");
 return names;
}
void caption(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& left, const juce::String& right = {}) {
 g.setFont(soft::monoFont(10.f));
 g.setColour(c::screenMuted);
 g.drawText(left, r.withHeight(12.f), juce::Justification::topLeft);
 if (right.isNotEmpty()) g.drawText(right, r.withHeight(12.f), juce::Justification::topRight);
}
} // namespace

float InversoHaloEditor::value(int param) const {
 return processor.apvts.getRawParameterValue(specs()[static_cast<std::size_t>(param)].id)->load();
}

InversoHaloEditor::InversoHaloEditor(InversoHaloProcessor& p) : AudioProcessorEditor(&p), processor(p) {
 setLookAndFeel(&look);
 body.title = "HALO";
 body.subtitle = juce::String::fromUTF8("80s REVERBS   \xc2\xb7   GATED, PLATE, HALL, ROOM");
 addAndMakeVisible(body);
 presets.getCount = [this] { return processor.getNumPrograms(); };
 presets.getCurrent = [this] { return processor.getCurrentProgram(); };
 presets.getName = [this](int i) { return processor.getProgramName(i); };
 presets.select = [this](int i) { processor.selectProgram(i); };
 presets.enableUserPresets(processor.apvts, "HALO");
 body.addAndMakeVisible(presets);
 using Page = soft::Module::Page;
 auto add = [&](juce::String title, juce::Colour accent, std::vector<Page> pages, int screen, std::vector<int> choices) {
  modules.push_back(std::make_unique<soft::Module>(processor.apvts, specs().data(), shortNames(), title, accent, std::move(pages), screen, std::move(choices)));
  body.addAndMakeVisible(*modules.back());
 };
 add("REVERB", neonViolet, {{"Reverb", {P::Mix, P::PreDelay, P::Size, P::Decay, P::Damp}, {P::Mix}}}, 150, {P::Mode});
 add("GATE", neonPink, {{"Gate", {P::GateThresh, P::GateHold, P::GateRelease}}}, 150, {});
 add("INPUT", neonCyan, {{"Into the reverb", {P::LowCut, P::HighCut}}}, 150, {});
 master = std::make_unique<soft::Master>(processor.apvts, specs()[P::Output], &specs()[P::Width], juce::String("WIDTH"), c::ink);
 body.addAndMakeVisible(*master);
 auto& m = modules;
 // REVERB: the decay as it would sound, cut by the gate in the gated room.
 m[0]->screen.draw = [this](juce::Graphics& g, juce::Rectangle<float> r, int) {
  auto a = r.withTrimmedBottom(34.f).reduced(4.f, 2.f);
  static const char* modes[4] = {"PLATE", "HALL", "ROOM", "GATED ROOM"};
  const int mode = juce::jlimit(0, 3, juce::roundToInt(value(P::Mode)));
  const float decay = value(P::Decay), pre = value(P::PreDelay);
  caption(g, a, modes[mode], abismo_ui::formatValue(specs()[P::Decay], decay) + "  PRE " + abismo_ui::formatValue(specs()[P::PreDelay], pre));
  auto plot = a.withTrimmedTop(16.f);
  const float span = mode == 3 ? 1.2f : std::max(1.f, decay * 1.2f);
  const float hold = value(P::GateHold) * .001f, rel = value(P::GateRelease) * .001f;
  juce::Path path;
  juce::Random rnd(11);
  for (int i = 0; i <= 260; ++i) {
   const float t = span * static_cast<float>(i) / 260.f - pre * .001f;
   float env = t < 0.f ? 0.f : std::exp(-t * 6.9f / std::max(.2f, decay));
   if (mode == 3) env *= t < .05f + hold ? 1.f : std::exp(-(t - .05f - hold) / std::max(.005f, rel * .3f));
   const float y = env * (.6f + .4f * rnd.nextFloat());
   const float x = plot.getX() + plot.getWidth() * static_cast<float>(i) / 260.f;
   path.addRectangle(x, plot.getBottom() - y * plot.getHeight(), plot.getWidth() / 260.f * .7f, y * plot.getHeight());
  }
  g.setColour(neonViolet.withAlpha(.85f));
  g.fillPath(path);
 };
 // GATE: whether it is open now, over the last few seconds.
 m[1]->screen.draw = [this](juce::Graphics& g, juce::Rectangle<float> r, int) {
  auto a = r.reduced(4.f, 2.f);
  const bool gated = juce::roundToInt(value(P::Mode)) == 3;
  caption(g, a, gated ? juce::String("OPENS AT ") + abismo_ui::formatValue(specs()[P::GateThresh], value(P::GateThresh)) : juce::String("ONLY IN THE GATED ROOM"),
          gated ? juce::String(processor.engine.gateNow.load() > .5f ? "OPEN" : "SHUT") : juce::String());
  auto plot = a.withTrimmedTop(16.f).reduced(0.f, 4.f);
  juce::Path path;
  const int n = static_cast<int>(gateHistory.size());
  for (int i = 0; i < n; ++i) {
   const float x = plot.getX() + plot.getWidth() * static_cast<float>(i) / static_cast<float>(n - 1);
   const float y = plot.getBottom() - std::max(0.f, gateHistory[static_cast<std::size_t>(i)]) * plot.getHeight();
   if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
  }
  soft::glowStroke(g, path, neonPink.withAlpha(gated ? 1.f : .3f));
 };
 // INPUT: the band that goes into the reverb.
 m[2]->screen.draw = [this](juce::Graphics& g, juce::Rectangle<float> r, int) {
  auto a = r.reduced(4.f, 2.f);
  const float lo = value(P::LowCut), hi = value(P::HighCut);
  caption(g, a, abismo_ui::formatValue(specs()[P::LowCut], lo), abismo_ui::formatValue(specs()[P::HighCut], hi));
  auto plot = a.withTrimmedTop(16.f).reduced(0.f, 6.f);
  juce::Path path;
  for (int i = 0; i <= 200; ++i) {
   const float t = static_cast<float>(i) / 200.f, f = 20.f * std::pow(1000.f, t);
   const float hp = f / std::sqrt(f * f + lo * lo), lp = 1.f / (1.f + (f / hi) * (f / hi));
   const float y = plot.getBottom() - hp * lp * plot.getHeight();
   const float x = plot.getX() + t * plot.getWidth();
   if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
  }
  soft::glowStroke(g, path, neonCyan);
 };
 setResizable(true, true);
 soft::sizeWindow(*this,"HALO",designW,designH,0.6f);
 startTimerHz(30);
}

InversoHaloEditor::~InversoHaloEditor() { setLookAndFeel(nullptr); }

void InversoHaloEditor::timerCallback() {
 presets.refresh();
 std::rotate(gateHistory.begin(), gateHistory.begin() + 1, gateHistory.end());
 gateHistory.back() = processor.engine.gateNow.load();
 for (auto& m : modules) m->screen.repaint();
 master->setReadout(processor.outputPeak.load(), juce::String("ZERO LATENCY"));
}

void InversoHaloEditor::resized() {
 body.setBounds(0, 0, designW, designH);
 body.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / designW));
 soft::rememberWindow(*this,"HALO",designW);
 presets.setBounds(designW / 2 - 200, 14, 400, 72);
 const int x0 = 10, y0 = 88, h = designH - y0 - 12, mainW = 820;
 modules[0]->setBounds(x0, y0, 420, h);
 modules[1]->setBounds(x0 + 420, y0, 220, h);
 modules[2]->setBounds(x0 + 640, y0, mainW - 640, h);
 master->setBounds(x0 + mainW, y0, designW - (x0 + mainW) - 10, h);
}
