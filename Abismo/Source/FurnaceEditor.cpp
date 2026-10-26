#include "FurnaceEditor.h"
#include "DSP/FurnaceAmp.h"
#include <cmath>

using namespace inverso_abismo::furnace;
namespace c = soft::colour;

namespace {
constexpr int designW = 1300, designH = 760;

const char* const* shortNames() {
 static const char* names[] = {"MODEL", "GATE", "TIGHT", "GAIN", "BASS", "MIDDLE", "TREBLE", "PRESENCE", "MASTER", "SAG",
                               "CABINET", "MIC", "POSITION", "ANGLE", "DISTANCE", "SPRING", "DI BLEND", "OUTPUT"};
 static_assert(sizeof(names) / sizeof(names[0]) == P::Count, "one short name per parameter");
 return names;
}

void caption(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& left, const juce::String& right = {}) {
 g.setFont(soft::monoFont(10.f));
 g.setColour(c::screenMuted);
 g.drawText(left, r.withHeight(12.f), juce::Justification::topLeft);
 if (right.isNotEmpty()) g.drawText(right, r.withHeight(12.f), juce::Justification::topRight);
}

// A frequency response on a log axis, 40 Hz - 16 kHz, `range` dB tall.
juce::Path response(juce::Rectangle<float> r, float topDb, float range, const std::function<double(double)>& db) {
 juce::Path p;
 for (int i = 0; i <= 160; ++i) {
  const double t = i / 160.0, f = 40.0 * std::pow(400.0, t);
  const float y = juce::jlimit(r.getY(), r.getBottom(), r.getY() + (topDb - static_cast<float>(db(f))) / range * r.getHeight());
  const float x = r.getX() + static_cast<float>(t) * r.getWidth();
  if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
 }
 return p;
}
void frequencyGrid(juce::Graphics& g, juce::Rectangle<float> r) {
 g.setColour(c::screenMuted.withAlpha(.18f));
 g.setFont(soft::monoFont(9.f));
 for (double f : {100.0, 1000.0, 10000.0}) {
  const float x = r.getX() + static_cast<float>(std::log(f / 40.0) / std::log(400.0)) * r.getWidth();
  g.drawVerticalLine(juce::roundToInt(x), r.getY(), r.getBottom());
  g.setColour(c::screenMuted.withAlpha(.5f));
  g.drawText(f >= 1000 ? juce::String(juce::roundToInt(f / 1000)) + "k" : juce::String(juce::roundToInt(f)), juce::roundToInt(x) + 3, juce::roundToInt(r.getBottom()) - 12, 30, 11, juce::Justification::left);
  g.setColour(c::screenMuted.withAlpha(.18f));
 }
}

const amp::StackParts& stackOf(int model) { return modelSpecs()[static_cast<std::size_t>(juce::jlimit(0, numModels - 1, model))].stack; }

// What each amp is, in one line, for the screen.
const char* modelLine(int m) {
 static const char* lines[] = {"BLACKFACE CLEAN, SCOOPED, LOTS OF HEADROOM", "TOP BOOST CHIME, CLASS A BREAKUP", "PLEXI CRUNCH, MIDS FORWARD",
                               "HOT-RODDED BRITISH LEAD", "TIGHT MODERN HIGH GAIN", "SILICON FUZZ INTO A CLEAN AMP",
                               "BIG VALVE BASS AMP, CLEAN", "BASS DRIVE, LOWS KEPT CLEAN", "A TIRED VALVE RADIO: CRACKLE AND DROPOUTS"};
 return lines[juce::jlimit(0, numModels - 1, m)];
}
}

float InversoFurnaceEditor::value(int param) const {
 return processor.apvts.getRawParameterValue(specs()[static_cast<std::size_t>(param)].id)->load();
}

InversoFurnaceEditor::InversoFurnaceEditor(InversoFurnaceProcessor& p) : AudioProcessorEditor(&p), processor(p) {
 setLookAndFeel(&look);
 body.title = "FURNACE";
 body.subtitle = juce::String::fromUTF8("GUITAR AND BASS AMPLIFIERS   \xc2\xb7   CABINETS FROM INVERSO CABS, MIC MOVES FREELY");
 addAndMakeVisible(body);
 presets.getCount = [this] { return processor.getNumPrograms(); };
 presets.getCurrent = [this] { return processor.getCurrentProgram(); };
 presets.getName = [this](int i) { return processor.getProgramName(i); };
 presets.select = [this](int i) { processor.setCurrentProgram(i); };
 presets.enableUserPresets(processor.apvts, "FURNACE");
 body.addAndMakeVisible(presets);

 using Page = soft::Module::Page;
 auto add = [&](juce::String title, juce::Colour accent, std::vector<Page> pages, int screenH, std::vector<int> choices) -> soft::Module& {
  modules.push_back(std::make_unique<soft::Module>(processor.apvts, specs().data(), shortNames(), title, accent, std::move(pages), screenH, std::move(choices)));
  body.addAndMakeVisible(*modules.back());
  return *modules.back();
 };
 add("INPUT", c::green, {{"Input", {P::Gate, P::Tight}}}, 120, {});
 add("AMPLIFIER", c::orange, {{"Amp", {P::Gain, P::Bass, P::Mid, P::Treble, P::Presence, P::Master, P::Sag}, {P::Gain}}}, 120, {P::Model});
 add("CABINET", c::amber, {{"Mic", {P::Position, P::Axis, P::Distance}, {P::Position}}}, 130, {P::Cab, P::Mic});
 add("AFTER", c::blue, {{"After", {P::Spring, P::DIBlend}}}, 130, {});
 master = std::make_unique<soft::Master>(processor.apvts, specs()[P::Output], nullptr, juce::String(), c::ink);
 body.addAndMakeVisible(*master);

 loadIR.setTooltip("Use an impulse response file of your own (WAV, AIFF, FLAC) as the cabinet");
 loadIR.onClick = [this] { chooseIR(); };
 modules[2]->addHeaderComponent(loadIR, 110);

 auto& m = modules;
 m[0]->screen.draw = [this](juce::Graphics& g, juce::Rectangle<float> r, int) {
  const float gateDb = value(P::Gate);
  const float inDb = juce::Decibels::gainToDecibels(processor.inputPeak.load(), -90.f);
  caption(g, r, "INPUT " + (inDb <= -89.f ? juce::String("-INF") : juce::String(juce::roundToInt(inDb)) + " dB"),
          gateDb <= -89.9f ? juce::String("GATE OFF") : processor.gateLevel.load() > .5f ? juce::String("GATE OPEN") : juce::String("GATE SHUT"));
  auto bar = r.withTrimmedTop(22.f).withHeight(18.f).reduced(2.f, 0.f);
  g.setColour(c::screenMuted.withAlpha(.2f));
  g.fillRoundedRectangle(bar, 3.f);
  const auto at = [&](float db) { return bar.getX() + (db + 90.f) / 90.f * bar.getWidth(); };
  g.setColour(c::green.withAlpha(.85f));
  g.fillRoundedRectangle(bar.withRight(at(juce::jlimit(-90.f, 0.f, inDb))), 3.f);
  if (gateDb > -89.9f) {
   g.setColour(c::screenInk);
   g.fillRect(at(gateDb) - 1.f, bar.getY() - 3.f, 2.f, bar.getHeight() + 6.f);
  }
  // the tight filter
  auto plot = r.withTrimmedTop(48.f).withTrimmedBottom(4.f);
  const double fc = 40.0 * std::pow(10.0, value(P::Tight) / 100.0);
  frequencyGrid(g, plot);
  soft::glowStroke(g, response(plot, 3.f, 40.f, [fc](double f) {
   const double x = f / fc;
   return 10 * std::log10(x * x * x * x / (1 + x * x * x * x));
  }), c::green);
  g.setFont(soft::monoFont(10.f));
  g.setColour(c::screenMuted);
  g.drawText("LOW CUT " + juce::String(juce::roundToInt(fc)) + " Hz", plot.withTrimmedTop(plot.getHeight() * .35f).withHeight(12.f), juce::Justification::centredRight);
 };
 m[1]->screen.draw = [this](juce::Graphics& g, juce::Rectangle<float> r, int) {
  const int model = juce::roundToInt(value(P::Model));
  auto area = r.withTrimmedBottom(34.f);
  caption(g, area, modelLine(model), "TONE STACK");
  auto plot = area.withTrimmedTop(16.f);
  frequencyGrid(g, plot);
  const auto& parts = stackOf(model);
  const double t = amp::taperTreble(value(P::Treble) / 10.0), l = amp::taperBass(value(P::Bass) / 10.0), mid = amp::taperMid(value(P::Mid) / 10.0);
  // the stack is passive: it only cuts; draw it against its own loudest point
  double top = -200;
  for (int i = 0; i <= 40; ++i) top = std::max(top, 20 * std::log10(amp::ToneStack::magnitude(parts, t, l, mid, 40.0 * std::pow(400.0, i / 40.0))));
  soft::glowStroke(g, response(plot.reduced(0.f, 4.f), static_cast<float>(top) + 3.f, 24.f, [&](double f) {
   return 20 * std::log10(amp::ToneStack::magnitude(parts, t, l, mid, f));
  }), c::orange);
 };
 m[2]->screen.draw = [this](juce::Graphics& g, juce::Rectangle<float> r, int) {
  auto area = r.withTrimmedBottom(34.f);
  const int cab = juce::roundToInt(value(P::Cab));
  if (cab == cabOff) { caption(g, area, "NO CABINET: THE AMP GOES OUT RAW (FOR AN IR LOADER AFTER FURNACE)"); return; }
  if (cab == cabUser) {
   const auto name = processor.userIRName();
   caption(g, area, name.isEmpty() ? juce::String("USER IR: NONE LOADED, PRESS LOAD IR") : "USER IR: " + name.toUpperCase());
   return;
  }
  const double pos = value(P::Position) / 100.0, axis = value(P::Axis) / 100.0, dist = value(P::Distance) / 100.0;
  const auto placement = fromKnobs(pos, axis, dist);
  caption(g, area, juce::String(placement.dist * 100.0, placement.dist < .1 ? 1 : 0) + " CM FROM THE GRILLE", "RESPONSE");
  // the speaker seen from the front, the mic over it
  auto cone = area.withTrimmedTop(16.f).removeFromLeft(area.getHeight() - 16.f).reduced(6.f);
  const auto centre = cone.getCentre();
  const float radius = cone.getWidth() * .5f;
  g.setColour(c::screenMuted.withAlpha(.25f));
  g.drawEllipse(cone, 1.5f);
  g.drawEllipse(cone.reduced(radius * .12f), 1.f);
  g.setColour(c::screenMuted.withAlpha(.4f));
  g.fillEllipse(juce::Rectangle<float>(radius * .5f, radius * .5f).withCentre(centre));
  const float micX = centre.x + static_cast<float>(pos) * radius * .85f;
  const float micSize = 4.f + 3.f * (1.f - static_cast<float>(dist));
  juce::Path micShape;
  micShape.addRoundedRectangle(-micSize * .5f, -micSize * 1.6f, micSize, micSize * 3.2f, micSize * .4f);
  micShape.applyTransform(juce::AffineTransform::rotation(static_cast<float>(axis) * .8f).translated(micX, centre.y));
  g.setColour(c::amber.withAlpha(.9f - .5f * static_cast<float>(dist)));
  g.fillPath(micShape);
  // its frequency response
  auto plot = area.withTrimmedTop(16.f).withTrimmedLeft(cone.getRight() - area.getX() + 14.f);
  frequencyGrid(g, plot);
  const int mic = juce::roundToInt(value(P::Mic));
  double top = -200;
  for (int i = 0; i <= 40; ++i) top = std::max(top, InversoFurnaceProcessor::cabResponseDb(cab, mic, pos, axis, dist, 40.0 * std::pow(400.0, i / 40.0)));
  soft::glowStroke(g, response(plot.reduced(0.f, 4.f), static_cast<float>(top) + 3.f, 45.f, [&](double f) {
   return InversoFurnaceProcessor::cabResponseDb(cab, mic, pos, axis, dist, f);
  }), c::amber);
 };
 m[3]->screen.draw = [this](juce::Graphics& g, juce::Rectangle<float> r, int) {
  const float spring = value(P::Spring) / 100.f, di = value(P::DIBlend) / 100.f;
  caption(g, r, "SPRING TANK", "DI " + juce::String(juce::roundToInt(di * 100.f)) + "%");
  auto tank = r.withTrimmedTop(20.f).withHeight(r.getHeight() * .45f).reduced(10.f, 4.f);
  juce::Path coil;
  const float wobble = spring * 3.f * std::sin(animation * 9.f) * processor.outputPeak.load();
  for (int i = 0; i <= 240; ++i) {
   const float t = static_cast<float>(i) / 240.f;
   const float x = tank.getX() + t * tank.getWidth();
   const float y = tank.getCentreY() + (tank.getHeight() * .35f + wobble) * std::sin(t * 6.2831853f * 22.f);
   if (i == 0) coil.startNewSubPath(x, y); else coil.lineTo(x, y);
  }
  soft::glowStroke(g, coil, c::blue.withAlpha(.25f + .75f * spring), 1.2f);
  // the blend between the amp and the clean DI
  auto blend = r.withTrimmedTop(tank.getBottom() - r.getY() + 16.f).withHeight(14.f).reduced(10.f, 0.f);
  g.setColour(c::screenMuted.withAlpha(.25f));
  g.fillRoundedRectangle(blend, 3.f);
  g.setColour(c::blue.withAlpha(.85f));
  g.fillRoundedRectangle(blend.withWidth(blend.getWidth() * (1.f - di)), 3.f);
  g.setFont(soft::monoFont(9.f));
  g.setColour(c::screenMuted);
  g.drawText("AMP", blend.translated(0, 14).withHeight(12), juce::Justification::left);
  g.drawText("DI", blend.translated(0, 14).withHeight(12), juce::Justification::right);
 };

 setResizable(true, true);
 soft::sizeWindow(*this,"FURNACE",designW,designH,0.6f);
 timerCallback();
 startTimerHz(30);
}

InversoFurnaceEditor::~InversoFurnaceEditor() { setLookAndFeel(nullptr); }

void InversoFurnaceEditor::chooseIR() {
 // start in INVERSO CABS when the installer put it there
 auto cabs = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("INVERSO").getChildFile("INVERSO CABS");
 chooser = std::make_unique<juce::FileChooser>("An impulse response for the cabinet", cabs.isDirectory() ? cabs : juce::File(), "*.wav;*.aif;*.aiff;*.flac");
 chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
  if (fc.getResult().existsAsFile()) processor.loadUserIR(fc.getResult());
 });
}

void InversoFurnaceEditor::timerCallback() {
 animation += 1.f / 30.f;
 presets.refresh();
 const int cab = juce::roundToInt(value(P::Cab));
 const bool designed = cab > cabOff && cab < cabUser;
 for (auto& m : modules) {
  m->dimInactive([&](int p) {
   if (p == P::Mic || p == P::Position || p == P::Axis || p == P::Distance) return designed;
   return true;
  });
  m->screen.repaint();
 }
 const float inDb = juce::Decibels::gainToDecibels(processor.inputPeak.load(), -90.f);
 master->setReadout(processor.outputPeak.load(), "IN " + (inDb <= -89.f ? juce::String("-INF") : juce::String(juce::roundToInt(inDb)) + " dB"));
}

void InversoFurnaceEditor::resized() {
 body.setBounds(0, 0, designW, designH);
 body.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / designW));
 soft::rememberWindow(*this,"FURNACE",designW);
 presets.setBounds(designW / 2 - 210, 14, 420, 72);
 const int x0 = 10, y0 = 88, mainW = 1110, r1 = 330, r2 = designH - y0 - r1 - 12;
 modules[0]->setBounds(x0, y0, 270, r1);
 modules[1]->setBounds(x0 + 270, y0, mainW - 270, r1);
 modules[2]->setBounds(x0, y0 + r1, 700, r2);
 modules[3]->setBounds(x0 + 700, y0 + r1, mainW - 700, r2);
 master->setBounds(x0 + mainW, y0, designW - (x0 + mainW) - 10, r1 + r2);
}
