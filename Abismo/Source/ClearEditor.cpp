#include "ClearEditor.h"
#include <cmath>

using namespace inverso_abismo::clear;
namespace c = soft::colour;

namespace {
constexpr int designW = 1240, designH = 780;
const char* const* shortNames() {
 static const char* names[] = {"DEPTH", "WHERE", "SENSITIVITY", "DEPTH", "FROM", "UP TO", "SENSITIVITY", "WIDTH", "SPEED", "MIX", "OUTPUT", "DELTA"};
 static_assert(sizeof(names) / sizeof(names[0]) == P::Count, "one short name per parameter");
 return names;
}
float xOf(float f, juce::Rectangle<float> r) {
 return r.getX() + std::log(f / Engine::displayLow) / std::log(Engine::displayHigh / Engine::displayLow) * r.getWidth();
}
}

void ClearAnalyzer::update() {
 auto& e = processor.engine;
 for (int i = 0; i < Engine::displayBins; ++i) {
  const auto k = static_cast<std::size_t>(i);
  level[k] = e.displayInput[k].load(std::memory_order_relaxed);
  const float r = e.displayReduction[k].load(std::memory_order_relaxed);
  cut[k] += .5f * (r - cut[k]);
  peakCut[k] = std::max(cut[k], peakCut[k] - .08f);
 }
 repaint();
}

void ClearAnalyzer::paint(juce::Graphics& g) {
 const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
 const int w = juce::roundToInt(static_cast<float>(getWidth()) * scale), h = juce::roundToInt(static_cast<float>(getHeight()) * scale);
 if (cardCache.getWidth() != w || cardCache.getHeight() != h) {
  cardCache = juce::Image(juce::Image::ARGB, std::max(1, w), std::max(1, h), true);
  juce::Graphics cg(cardCache);
  cg.addTransform(juce::AffineTransform::scale(scale));
  soft::paintCard(cg, getLocalBounds().toFloat().reduced(10.f));
 }
 g.drawImageTransformed(cardCache, juce::AffineTransform::scale(1.f / scale));
 auto inner = getLocalBounds().toFloat().reduced(10.f).reduced(16.f, 12.f);
 g.setColour(c::cyan);
 g.fillEllipse(inner.getX(), inner.getY() + 6.f, 8.f, 8.f);
 g.setColour(c::ink);
 g.setFont(soft::titleFont(12.5f));
 g.drawText("WHAT CLEAR TAKES AWAY", inner.withHeight(20.f).withTrimmedLeft(16.f), juce::Justification::centredLeft);
 auto& apvts = processor.apvts;
 auto v = [&](int p) { return apvts.getRawParameterValue(specs()[static_cast<std::size_t>(p)].id)->load(); };
 g.setFont(soft::monoFont(10.f));
 g.setColour(c::muted);
 g.drawText("MUD " + juce::String(processor.engine.mudNow.load(), 1) + " dB   HARSHNESS " + juce::String(processor.engine.harshNow.load(), 1) + " dB",
            inner.withHeight(20.f), juce::Justification::centredRight);
 inner.removeFromTop(30.f);
 soft::paintScreen(g, inner);
 auto plot = inner.reduced(14.f, 12.f);
 const auto spectrum = plot.withTrimmedBottom(plot.getHeight() * .38f);
 const auto cuts = plot.withTrimmedTop(plot.getHeight() * .66f).withTrimmedBottom(14.f);
 // grid
 g.setFont(soft::monoFont(9.f));
 for (float f : {50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f}) {
  const float x = xOf(f, plot);
  g.setColour(c::screenMuted.withAlpha(.15f));
  g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom() - 12.f);
  g.setColour(c::screenMuted.withAlpha(.6f));
  g.drawText(f >= 1000 ? juce::String(juce::roundToInt(f / 1000)) + "k" : juce::String(juce::roundToInt(f)), juce::roundToInt(x) - 20, juce::roundToInt(plot.getBottom()) - 11, 40, 11, juce::Justification::centred);
 }
 // where each part works
 const float lo = v(P::HarshLow), hi = std::max(v(P::HarshHigh), lo * 1.1f), focus = v(P::MudFocus);
 if (v(P::HarshDepth) > 0) {
  g.setColour(c::pink.withAlpha(.08f));
  g.fillRect(juce::Rectangle<float>(xOf(lo, plot), plot.getY(), xOf(hi, plot) - xOf(lo, plot), plot.getHeight() - 12.f));
 }
 if (v(P::MudDepth) > 0) {
  g.setColour(c::amber.withAlpha(.08f));
  g.fillRect(juce::Rectangle<float>(xOf(focus / 1.5f, plot), plot.getY(), xOf(focus * 1.5f, plot) - xOf(focus / 1.5f, plot), plot.getHeight() - 12.f));
 }
 // the spectrum, -90 .. 0 dBFS
 juce::Path s, sFill;
 const int n = Engine::displayBins;
 for (int i = 0; i < n; ++i) {
  const float x = spectrum.getX() + (static_cast<float>(i) + .5f) / n * spectrum.getWidth();
  const float y = juce::jmap(juce::jlimit(-90.f, 0.f, level[static_cast<std::size_t>(i)]), -90.f, 0.f, spectrum.getBottom(), spectrum.getY());
  if (i == 0) { s.startNewSubPath(x, y); sFill.startNewSubPath(x, spectrum.getBottom()); }
  else s.lineTo(x, y);
  sFill.lineTo(x, y);
 }
 sFill.lineTo(spectrum.getRight(), spectrum.getBottom());
 sFill.closeSubPath();
 g.setColour(c::cyan.withAlpha(.10f));
 g.fillPath(sFill);
 soft::glowStroke(g, s, c::cyan.withAlpha(.8f), 1.2f);
 // what goes: bars hanging from a line, 0 .. 12 dB
 g.setColour(c::screenMuted.withAlpha(.3f));
 g.drawHorizontalLine(juce::roundToInt(cuts.getY()), cuts.getX(), cuts.getRight());
 g.setColour(c::screenMuted.withAlpha(.6f));
 g.drawText("0 dB", cuts.withHeight(10.f).translated(0, -12.f), juce::Justification::topLeft);
 g.drawText("-12 dB", cuts.withTrimmedTop(cuts.getHeight() - 10.f), juce::Justification::bottomLeft);
 const float bw = cuts.getWidth() / n;
 for (int i = 0; i < n; ++i) {
  const float r = cut[static_cast<std::size_t>(i)], pk = peakCut[static_cast<std::size_t>(i)];
  const float x = cuts.getX() + static_cast<float>(i) * bw;
  const float freq = Engine::displayLow * std::pow(Engine::displayHigh / Engine::displayLow, (static_cast<float>(i) + .5f) / n);
  const auto colour = freq < 800.f ? c::amber : c::pink;
  if (r > .05f) {
   g.setColour(colour.withAlpha(.85f));
   g.fillRect(x + .5f, cuts.getY(), bw - 1.f, std::min(1.f, r / 12.f) * cuts.getHeight());
  }
  if (pk > .1f) {
   g.setColour(colour.withAlpha(.5f));
   g.fillRect(x + .5f, cuts.getY() + std::min(1.f, pk / 12.f) * cuts.getHeight() - 1.f, bw - 1.f, 1.5f);
  }
 }
}

InversoClearEditor::InversoClearEditor(InversoClearProcessor& p) : AudioProcessorEditor(&p), processor(p), analyzer(p) {
 setLookAndFeel(&look);
 body.title = "CLEAR";
 body.subtitle = juce::String::fromUTF8("TAKES THE MUD AND THE HARSHNESS OUT ONLY WHEN THEY ARE THERE   \xc2\xb7   DELTA: HEAR WHAT GOES");
 addAndMakeVisible(body);
 presets.getCount = [this] { return processor.getNumPrograms(); };
 presets.getCurrent = [this] { return processor.getCurrentProgram(); };
 presets.getName = [this](int i) { return processor.getProgramName(i); };
 presets.select = [this](int i) { processor.setCurrentProgram(i); };
 body.addAndMakeVisible(presets);
 body.addAndMakeVisible(analyzer);
 using Page = soft::Module::Page;
 auto add = [&](juce::String title, juce::Colour accent, std::vector<Page> pages) {
  modules.push_back(std::make_unique<soft::Module>(processor.apvts, specs().data(), shortNames(), title, accent, std::move(pages), 0));
  body.addAndMakeVisible(*modules.back());
 };
 add("MUD", c::amber, {{"Mud", {P::MudDepth, P::MudFocus, P::MudSense}, {P::MudDepth}}});
 add("HARSHNESS", c::pink, {{"Harshness", {P::HarshDepth, P::HarshLow, P::HarshHigh, P::HarshSense, P::Sharpness}, {P::HarshDepth}}});
 add("RESPONSE", c::cyan, {{"Response", {P::Speed, P::Mix, P::Delta}}});
 master = std::make_unique<soft::Master>(processor.apvts, specs()[P::Output], nullptr, juce::String(), c::ink);
 body.addAndMakeVisible(*master);
 setResizable(true, true);
 setResizeLimits(designW * 6 / 10, designH * 6 / 10, designW * 3 / 2, designH * 3 / 2);
 getConstrainer()->setFixedAspectRatio(static_cast<double>(designW) / designH);
 setSize(designW * 9 / 10, designH * 9 / 10);
 startTimerHz(30);
}

InversoClearEditor::~InversoClearEditor() { setLookAndFeel(nullptr); }

void InversoClearEditor::timerCallback() {
 presets.refresh();
 analyzer.update();
 const bool delta = processor.apvts.getRawParameterValue("DELTA")->load() >= .5f;
 master->setReadout(processor.outputPeak.load(), delta ? juce::String("LISTENING TO WHAT GOES") : juce::String("LATENCY ") + juce::String(processor.getLatencySamples()));
}

void InversoClearEditor::resized() {
 body.setBounds(0, 0, designW, designH);
 body.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / designW));
 presets.setBounds(designW / 2 - 210, 14, 420, 72);
 const int x0 = 10, y0 = 88, mainW = 1060, top = 380, bottom = designH - y0 - top - 12;
 analyzer.setBounds(x0, y0, mainW, top);
 modules[0]->setBounds(x0, y0 + top, 340, bottom);
 modules[1]->setBounds(x0 + 340, y0 + top, 480, bottom);
 modules[2]->setBounds(x0 + 820, y0 + top, mainW - 820, bottom);
 master->setBounds(x0 + mainW, y0, designW - (x0 + mainW) - 10, top + bottom);
}
