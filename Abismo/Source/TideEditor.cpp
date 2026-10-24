#include "TideEditor.h"
#include <cmath>

using namespace inverso_abismo::tide;
namespace c = soft::colour;

namespace {
constexpr int designW = 1400, designH = 800;
constexpr double fLow = 20, fHigh = 20000, dbSpan = 20; // the curve's scale: +-20 dB
constexpr float specLow = -90.f, specHigh = -10.f;      // the analyser's scale
const char* const* shortNames() {
 static const char* names[] = {
  "ON", "FREQ", "GAIN", "SLOPE", "THRESHOLD", "RANGE",
  "ON", "FREQ", "GAIN", "WIDTH", "THRESHOLD", "RANGE",
  "ON", "FREQ", "GAIN", "WIDTH", "THRESHOLD", "RANGE",
  "ON", "FREQ", "GAIN", "WIDTH", "THRESHOLD", "RANGE",
  "ON", "FREQ", "GAIN", "SLOPE", "THRESHOLD", "RANGE",
  "LOW CUT", "FROM", "HIGH CUT", "FROM", "ATTACK", "RELEASE", "OUTPUT"};
 static_assert(sizeof(names) / sizeof(names[0]) == P::Count, "one short name per parameter");
 return names;
}
juce::Colour bandColour(int k) {
 static const juce::Colour colours[numBands] = {c::amber, c::green, c::cyan, c::violet, c::pink};
 return colours[k];
}
const char* bandTitle(int k) {
 static const char* titles[numBands] = {"LOW SHELF", "BAND 2", "BAND 3", "BAND 4", "HIGH SHELF"};
 return titles[k];
}
}

TideGraph::TideGraph(InversoTideProcessor& p) : processor(p) {
 const int n = 1 << 12;
 window.resize(static_cast<std::size_t>(n));
 for (int i = 0; i < n; ++i) window[static_cast<std::size_t>(i)] = .5f - .5f * std::cos(2.f * juce::MathConstants<float>::pi * static_cast<float>(i) / static_cast<float>(n));
 work.resize(static_cast<std::size_t>(2 * n));
 setRepaintsOnMouseActivity(false);
}

juce::Rectangle<float> TideGraph::plotArea() const {
 return getLocalBounds().toFloat().reduced(10.f).reduced(16.f, 12.f).withTrimmedTop(30.f).reduced(14.f, 12.f).withTrimmedBottom(12.f);
}
float TideGraph::xOf(double f) const {
 const auto r = plotArea();
 return r.getX() + static_cast<float>(std::log(f / fLow) / std::log(fHigh / fLow)) * r.getWidth();
}
double TideGraph::fOf(float x) const {
 const auto r = plotArea();
 return fLow * std::pow(fHigh / fLow, juce::jlimit(0.0, 1.0, static_cast<double>((x - r.getX()) / r.getWidth())));
}
float TideGraph::yOf(double db) const {
 const auto r = plotArea();
 return r.getCentreY() - static_cast<float>(db / dbSpan) * r.getHeight() * .5f;
}
double TideGraph::dbOf(float y) const {
 const auto r = plotArea();
 return (r.getCentreY() - y) / (r.getHeight() * .5f) * dbSpan;
}
float TideGraph::value(int param) const {
 return processor.apvts.getRawParameterValue(specs()[static_cast<std::size_t>(param)].id)->load();
}
void TideGraph::set(int param, float v) {
 if (auto* p = processor.apvts.getParameter(specs()[static_cast<std::size_t>(param)].id)) p->setValueNotifyingHost(p->convertTo0to1(v));
}
void TideGraph::gesture(int band, bool begin) {
 for (int f : {P::Freq, P::Gain, P::Q, P::Range})
  if (auto* p = processor.apvts.getParameter(specs()[static_cast<std::size_t>(P::band(band, f))].id)) begin ? p->beginChangeGesture() : p->endChangeGesture();
}
juce::Point<float> TideGraph::nodeOf(int k) const {
 return {xOf(value(P::band(k, P::Freq))), yOf(value(P::band(k, P::Gain)))};
}
int TideGraph::hit(juce::Point<float> p) const {
 int best = -1;
 float bestD = 14.f;
 for (int k = 0; k < numBands; ++k) {
  const float d = nodeOf(k).getDistanceFrom(p);
  if (d < bestD) { bestD = d; best = k; }
 }
 return best;
}

void TideGraph::mouseMove(const juce::MouseEvent& e) {
 const int h = hit(e.position);
 if (h != hover) { hover = h; repaint(); }
 setMouseCursor(h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}
void TideGraph::mouseDown(const juce::MouseEvent& e) {
 dragging = hit(e.position);
 if (dragging < 0) return;
 dragRange = e.mods.isAltDown() || e.mods.isPopupMenu();
 gesture(dragging, true);
 if (value(P::band(dragging, P::On)) < .5f) set(P::band(dragging, P::On), 1.f);
 if (onSelect) onSelect(dragging);
}
void TideGraph::mouseDrag(const juce::MouseEvent& e) {
 if (dragging < 0) return;
 const auto& freqSpec = specs()[static_cast<std::size_t>(P::band(dragging, P::Freq))];
 if (dragRange) {
  // up/down from the node sets the dynamic range (an arrow from the gain)
  const float range = static_cast<float>(dbOf(e.position.y) - value(P::band(dragging, P::Gain)));
  set(P::band(dragging, P::Range), juce::jlimit(-18.f, 18.f, std::round(range * 2.f) / 2.f));
  return;
 }
 set(P::band(dragging, P::Freq), juce::jlimit(freqSpec.min, freqSpec.max, static_cast<float>(fOf(e.position.x))));
 set(P::band(dragging, P::Gain), juce::jlimit(-18.f, 18.f, static_cast<float>(dbOf(e.position.y))));
}
void TideGraph::mouseUp(const juce::MouseEvent&) {
 if (dragging >= 0) gesture(dragging, false);
 dragging = -1;
}
void TideGraph::mouseDoubleClick(const juce::MouseEvent& e) {
 const int k = hit(e.position);
 if (k < 0) return;
 gesture(k, true);
 set(P::band(k, P::Gain), 0.f);
 set(P::band(k, P::Range), 0.f);
 gesture(k, false);
}
void TideGraph::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
 const int k = hover >= 0 ? hover : hit(e.position);
 if (k < 0) return;
 const auto& spec = specs()[static_cast<std::size_t>(P::band(k, P::Q))];
 const float q = value(P::band(k, P::Q)) * std::pow(2.f, w.deltaY * (w.isReversed ? -1.f : 1.f));
 gesture(k, true);
 set(P::band(k, P::Q), juce::jlimit(spec.min, spec.max, q));
 gesture(k, false);
}

void TideGraph::update() {
 auto& e = processor.engine;
 for (int k = 0; k < numBands; ++k) dynamic[static_cast<std::size_t>(k)] = e.dynamicNow[static_cast<std::size_t>(k)].load(std::memory_order_relaxed);
 // the spectrum of the last 4096 samples
 const int n = 1 << 12;
 if (static_cast<int>(e.scope.size()) == Engine::scopeSize) {
  const int w = e.scopeWrite.load(std::memory_order_relaxed);
  for (int i = 0; i < n; ++i) {
   const int at = (w - n + i + Engine::scopeSize) % Engine::scopeSize;
   work[static_cast<std::size_t>(i)] = e.scope[static_cast<std::size_t>(at)] * window[static_cast<std::size_t>(i)];
  }
  std::fill(work.begin() + n, work.end(), 0.f);
  fft.performFrequencyOnlyForwardTransform(work.data());
  const auto r = plotArea();
  const int columns = std::max(2, static_cast<int>(r.getWidth() / 3.f));
  if (static_cast<int>(spectrum.size()) != columns) spectrum.assign(static_cast<std::size_t>(columns), specLow);
  const double sr = processor.getSampleRate() > 0 ? processor.getSampleRate() : 48000.0;
  for (int col = 0; col < columns; ++col) {
   const double f0 = fLow * std::pow(fHigh / fLow, static_cast<double>(col) / columns);
   const double f1 = fLow * std::pow(fHigh / fLow, static_cast<double>(col + 1) / columns);
   int b0 = static_cast<int>(f0 / sr * n), b1 = static_cast<int>(f1 / sr * n);
   b0 = juce::jlimit(1, n / 2 - 1, b0); b1 = juce::jlimit(b0, n / 2 - 1, b1);
   float m = 0.f;
   for (int b = b0; b <= b1; ++b) m = std::max(m, work[static_cast<std::size_t>(b)]);
   // a sine at full scale reads 0 dB; then a 4.5 dB/oct tilt so music looks level
   const float db = 20.f * std::log10(m / (n * .25f) + 1e-9f) + 4.5f * static_cast<float>(std::log2(std::sqrt(f0 * f1) / 1000.0));
   auto& s = spectrum[static_cast<std::size_t>(col)];
   s = db > s ? s + .6f * (db - s) : s + .15f * (db - s);
  }
 }
 repaint();
}

void TideGraph::paint(juce::Graphics& g) {
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
 g.drawText("THE CURVE AS SET, AND AS IT MOVES", inner.withHeight(20.f).withTrimmedLeft(16.f), juce::Justification::centredLeft);
 g.setFont(soft::monoFont(10.f));
 g.setColour(c::muted);
 g.drawText(juce::String::fromUTF8("DRAG: FREQUENCY / GAIN   \xc2\xb7   ALT OR RIGHT-DRAG: DYNAMIC RANGE   \xc2\xb7   WHEEL: WIDTH   \xc2\xb7   DOUBLE-CLICK: FLAT"),
            inner.withHeight(20.f), juce::Justification::centredRight);
 inner.removeFromTop(30.f);
 soft::paintScreen(g, inner);
 const auto plot = plotArea();
 // grid
 g.setFont(soft::monoFont(9.f));
 for (double f : {30.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0}) {
  const float x = xOf(f);
  g.setColour(c::screenMuted.withAlpha(.14f));
  g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
  g.setColour(c::screenMuted.withAlpha(.6f));
  g.drawText(f >= 1000 ? juce::String(juce::roundToInt(f / 1000)) + "k" : juce::String(juce::roundToInt(f)), juce::roundToInt(x) - 20, juce::roundToInt(plot.getBottom()) + 1, 40, 11, juce::Justification::centred);
 }
 for (double db : {-18.0, -12.0, -6.0, 0.0, 6.0, 12.0, 18.0}) {
  const float y = yOf(db);
  g.setColour(c::screenMuted.withAlpha(db == 0 ? .35f : .12f));
  g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
  g.setColour(c::screenMuted.withAlpha(.55f));
  g.drawText((db > 0 ? "+" : "") + juce::String(juce::roundToInt(db)), juce::roundToInt(plot.getX()) + 2, juce::roundToInt(y) - 11, 30, 10, juce::Justification::left);
 }
 // the analyser, under everything
 if (!spectrum.empty()) {
  juce::Path s, fill;
  const int n = static_cast<int>(spectrum.size());
  for (int i = 0; i < n; ++i) {
   const float x = plot.getX() + (static_cast<float>(i) + .5f) / n * plot.getWidth();
   const float y = juce::jmap(juce::jlimit(specLow, specHigh, spectrum[static_cast<std::size_t>(i)]), specLow, specHigh, plot.getBottom(), plot.getY());
   if (i == 0) { s.startNewSubPath(x, y); fill.startNewSubPath(x, plot.getBottom()); }
   else s.lineTo(x, y);
   fill.lineTo(x, y);
  }
  fill.lineTo(plot.getRight(), plot.getBottom());
  fill.closeSubPath();
  g.setColour(c::screenMuted.withAlpha(.12f));
  g.fillPath(fill);
  g.setColour(c::screenMuted.withAlpha(.35f));
  g.strokePath(s, juce::PathStrokeType(1.f));
 }
 Values v{};
 for (std::size_t i = 0; i < v.size(); ++i) v[i] = value(static_cast<int>(i));
 v[P::Output] = 0.f; // the curve shows the EQ, not the output level
 const std::array<float, numBands> none{};
 auto curve = [&](const std::array<float, numBands>& dyn) {
  juce::Path p;
  const int steps = static_cast<int>(plot.getWidth() / 2.f);
  for (int i = 0; i <= steps; ++i) {
   const float x = plot.getX() + plot.getWidth() * static_cast<float>(i) / steps;
   const float y = juce::jlimit(plot.getY(), plot.getBottom(), yOf(Engine::responseDb(v, dyn, fOf(x))));
   if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
  }
  return p;
 };
 // each band's own contribution, faintly
 for (int k = 0; k < numBands; ++k) {
  if (v[static_cast<std::size_t>(P::band(k, P::On))] < .5f) continue;
  Values only = defaults();
  only[P::Output] = 0.f;
  for (int f = 0; f < P::BandStride; ++f) only[static_cast<std::size_t>(P::band(k, f))] = v[static_cast<std::size_t>(P::band(k, f))];
  only[P::LowCutOn] = only[P::HighCutOn] = 0.f;
  for (int o = 0; o < numBands; ++o) if (o != k) only[static_cast<std::size_t>(P::band(o, P::On))] = 0.f;
  juce::Path p;
  const int steps = static_cast<int>(plot.getWidth() / 3.f);
  for (int i = 0; i <= steps; ++i) {
   const float x = plot.getX() + plot.getWidth() * static_cast<float>(i) / steps;
   const float y = yOf(Engine::responseDb(only, dynamic, fOf(x)));
   if (i == 0) p.startNewSubPath(x, yOf(0)); p.lineTo(x, juce::jlimit(plot.getY(), plot.getBottom(), y));
  }
  p.lineTo(plot.getRight(), yOf(0));
  p.closeSubPath();
  g.setColour(bandColour(k).withAlpha(k == hover || k == dragging ? .22f : .10f));
  g.fillPath(p);
 }
 // the curve as set, and the curve as it is right now
 bool moving = false;
 for (float d : dynamic) moving = moving || std::abs(d) > .05f;
 const auto set = curve(none);
 g.setColour(c::screenInk.withAlpha(moving ? .45f : .9f));
 g.strokePath(set, juce::PathStrokeType(moving ? 1.2f : 2.f));
 if (moving) soft::glowStroke(g, curve(dynamic), c::cyan, 2.f);
 // the nodes, with the reach of their dynamic range
 for (int k = 0; k < numBands; ++k) {
  const bool on = v[static_cast<std::size_t>(P::band(k, P::On))] >= .5f;
  const auto colour = bandColour(k).withAlpha(on ? 1.f : .35f);
  const auto at = nodeOf(k);
  const float range = v[static_cast<std::size_t>(P::band(k, P::Range))];
  if (on && std::abs(range) > .01f) {
   const float y1 = juce::jlimit(plot.getY(), plot.getBottom(), yOf(v[static_cast<std::size_t>(P::band(k, P::Gain))] + range));
   g.setColour(colour.withAlpha(.5f));
   const float dash[] = {3.f, 3.f};
   g.drawDashedLine(juce::Line<float>(at.x, at.y, at.x, y1), dash, 2, 1.5f);
   g.drawLine(at.x - 6.f, y1, at.x + 6.f, y1, 1.5f);
   // where it is now
   const float yNow = juce::jlimit(plot.getY(), plot.getBottom(), yOf(v[static_cast<std::size_t>(P::band(k, P::Gain))] + dynamic[static_cast<std::size_t>(k)]));
   g.setColour(colour);
   g.fillEllipse(at.x - 3.f, yNow - 3.f, 6.f, 6.f);
  }
  const float r = k == hover || k == dragging ? 10.f : 8.f;
  g.setColour(colour.withAlpha(on ? .25f : .1f));
  g.fillEllipse(at.x - r - 3.f, at.y - r - 3.f, 2.f * (r + 3.f), 2.f * (r + 3.f));
  g.setColour(colour);
  g.fillEllipse(at.x - r, at.y - r, 2.f * r, 2.f * r);
  g.setColour(c::screenBottom);
  g.setFont(soft::uiFont(10.f, true));
  g.drawText(juce::String(k + 1), juce::Rectangle<float>(at.x - r, at.y - r, 2.f * r, 2.f * r), juce::Justification::centred);
 }
 // the hovered band's numbers
 const int show = dragging >= 0 ? dragging : hover;
 if (show >= 0) {
  const auto& fs = specs()[static_cast<std::size_t>(P::band(show, P::Freq))];
  juce::String text = juce::String(bandTitle(show)) + "   " + abismo_ui::formatValue(fs, v[static_cast<std::size_t>(P::band(show, P::Freq))]) + "   " +
                      abismo_ui::formatValue(specs()[static_cast<std::size_t>(P::band(show, P::Gain))], v[static_cast<std::size_t>(P::band(show, P::Gain))]) + "   " +
                      abismo_ui::formatValue(specs()[static_cast<std::size_t>(P::band(show, P::Q))], v[static_cast<std::size_t>(P::band(show, P::Q))]);
  const float range = v[static_cast<std::size_t>(P::band(show, P::Range))];
  if (std::abs(range) > .01f) text << "   RANGE " << (range > 0 ? "+" : "") << juce::String(range, 1) << " dB OVER " << juce::String(juce::roundToInt(v[static_cast<std::size_t>(P::band(show, P::Thresh))])) << " dB";
  g.setColour(bandColour(show));
  g.setFont(soft::monoFont(10.5f));
  g.drawText(text, plot.withHeight(16.f).translated(36.f, 2.f), juce::Justification::centredLeft);
 }
}

InversoTideEditor::InversoTideEditor(InversoTideProcessor& p) : AudioProcessorEditor(&p), processor(p), graph(p) {
 setLookAndFeel(&look);
 body.title = "TIDE";
 body.subtitle = juce::String::fromUTF8("A DYNAMIC EQ FOR INSTRUMENTS   \xc2\xb7   EACH BAND CAN MOVE ONLY WHEN ITS PART OF THE SOUND GETS LOUD");
 addAndMakeVisible(body);
 presets.getCount = [this] { return processor.getNumPrograms(); };
 presets.getCurrent = [this] { return processor.getCurrentProgram(); };
 presets.getName = [this](int i) { return processor.getProgramName(i); };
 presets.select = [this](int i) { processor.setCurrentProgram(i); };
 presets.enableUserPresets(processor.apvts, "TIDE");
 body.addAndMakeVisible(presets);
 body.addAndMakeVisible(graph);
 using Page = soft::Module::Page;
 auto add = [&](juce::String title, juce::Colour accent, std::vector<Page> pages) {
  modules.push_back(std::make_unique<soft::Module>(processor.apvts, specs().data(), shortNames(), title, accent, std::move(pages), 0));
  body.addAndMakeVisible(*modules.back());
 };
 for (int k = 0; k < numBands; ++k)
  add(bandTitle(k), bandColour(k), {{"EQ", {P::band(k, P::Freq), P::band(k, P::Gain), P::band(k, P::Q)}, {P::band(k, P::Gain)}},
                                    {"Dynamic", {P::band(k, P::Thresh), P::band(k, P::Range)}, {P::band(k, P::Range)}}});
 add("CUTS", c::blue, {{"Cuts", {P::LowCutOn, P::LowCut, P::HighCutOn, P::HighCut}}});
 add("RESPONSE", c::ink.brighter(.6f), {{"Response", {P::Attack, P::Release}}});
 master = std::make_unique<soft::Master>(processor.apvts, specs()[P::Output], nullptr, juce::String(), c::ink);
 body.addAndMakeVisible(*master);
 for (int k = 0; k < numBands; ++k) {
  auto& b = onButtons[static_cast<std::size_t>(k)];
  b.setClickingTogglesState(true);
  b.setTooltip("Switch this band on or off (its settings stay)");
  b.setColour(juce::TextButton::textColourOnId, c::screenInk);
  b.setColour(juce::TextButton::textColourOffId, c::muted);
  b.setColour(juce::TextButton::buttonOnColourId, bandColour(k).darker(.4f));
  b.setColour(juce::TextButton::buttonColourId, c::track);
  b.onStateChange = [&b] { const auto text = b.getToggleState() ? "ON" : "OFF"; if (b.getButtonText() != text) b.setButtonText(text); };
  onAttach[static_cast<std::size_t>(k)] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, specs()[static_cast<std::size_t>(P::band(k, P::On))].id, b);
  b.onStateChange();
  modules[static_cast<std::size_t>(k)]->addHeaderComponent(b, 36);
 }
 graph.onSelect = [this](int) {};
 setResizable(true, true);
 setResizeLimits(designW * 5 / 10, designH * 5 / 10, designW * 3 / 2, designH * 3 / 2);
 getConstrainer()->setFixedAspectRatio(static_cast<double>(designW) / designH);
 setSize(designW * 8 / 10, designH * 8 / 10);
 startTimerHz(30);
}

InversoTideEditor::~InversoTideEditor() { setLookAndFeel(nullptr); }

void InversoTideEditor::timerCallback() {
 presets.refresh();
 graph.update();
 for (int k = 0; k < numBands; ++k)
  modules[static_cast<std::size_t>(k)]->dimInactive([this, k](int param) {
   if (processor.apvts.getRawParameterValue(specs()[static_cast<std::size_t>(P::band(k, P::On))].id)->load() < .5f) return false;
   // the threshold only matters for a dynamic band
   if (param == P::band(k, P::Thresh)) return std::abs(processor.apvts.getRawParameterValue(specs()[static_cast<std::size_t>(P::band(k, P::Range))].id)->load()) > .01f;
   return true;
  });
 float moving = 0.f;
 for (auto& d : processor.engine.dynamicNow) moving = std::max(moving, std::abs(d.load()));
 master->setReadout(processor.outputPeak.load(), moving > .05f ? juce::String("MOVING ") + juce::String(moving, 1) + " dB" : juce::String("ZERO LATENCY"));
}

void InversoTideEditor::resized() {
 body.setBounds(0, 0, designW, designH);
 body.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / designW));
 presets.setBounds(designW / 2 - 210, 14, 420, 72);
 const int x0 = 10, y0 = 88, mainW = 1230, top = 420, bottom = designH - y0 - top - 12;
 graph.setBounds(x0, y0, mainW, top);
 int x = x0;
 for (int k = 0; k < numBands; ++k) { modules[static_cast<std::size_t>(k)]->setBounds(x, y0 + top, 182, bottom); x += 182; }
 modules[numBands]->setBounds(x, y0 + top, 200, bottom); x += 200;
 modules[numBands + 1]->setBounds(x, y0 + top, x0 + mainW - x, bottom);
 master->setBounds(x0 + mainW, y0, designW - (x0 + mainW) - 10, top + bottom);
}
