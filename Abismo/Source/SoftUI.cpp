#include "SoftUI.h"
#include "AbismoFonts.h"
#include <algorithm>
#include <cmath>

using namespace inverso_abismo;

namespace soft {

namespace {
juce::Typeface::Ptr load(const char* data, int size) { return juce::Typeface::createSystemTypefaceFor(data, static_cast<size_t>(size)); }
juce::Typeface::Ptr medium() { static auto t = load(AbismoFonts::PoppinsMedium_ttf, AbismoFonts::PoppinsMedium_ttfSize); return t; }
juce::Typeface::Ptr semibold() { static auto t = load(AbismoFonts::PoppinsSemiBold_ttf, AbismoFonts::PoppinsSemiBold_ttfSize); return t; }
juce::Typeface::Ptr mono() { static auto t = load(AbismoFonts::IBMPlexMonoMedium_ttf, AbismoFonts::IBMPlexMonoMedium_ttfSize); return t; }
constexpr float cardMargin = 10.f;
}

juce::Font uiFont(float height, bool bold) { return juce::Font(juce::FontOptions(bold ? semibold() : medium()).withHeight(height)); }
juce::Font monoFont(float height) { return juce::Font(juce::FontOptions(mono()).withHeight(height)); }

// ---------------------------------------------------------------- drawing helpers

void paintCard(juce::Graphics& g, juce::Rectangle<float> r, float radius) {
 juce::Path p;
 p.addRoundedRectangle(r, radius);
 juce::DropShadow(colour::shadow.withAlpha(.42f), 22, {7, 9}).drawForPath(g, p);
 juce::DropShadow(juce::Colours::white.withAlpha(.95f), 16, {-6, -6}).drawForPath(g, p);
 g.setGradientFill(juce::ColourGradient(colour::card.brighter(.25f), r.getX(), r.getY(), colour::card.darker(.025f), r.getX(), r.getBottom(), false));
 g.fillPath(p);
 g.setColour(juce::Colours::white.withAlpha(.9f));
 g.strokePath(p, juce::PathStrokeType(1.f));
}

void paintScreen(juce::Graphics& g, juce::Rectangle<float> r, float radius) {
 // A sunken frame first, then the glass.
 juce::Path frame;
 frame.addRoundedRectangle(r.expanded(2.f), radius + 2.f);
 g.setColour(colour::shadow.withAlpha(.25f));
 g.fillPath(frame);
 g.setGradientFill(juce::ColourGradient(colour::screenTop, r.getX(), r.getY(), colour::screenBottom, r.getX(), r.getBottom(), false));
 g.fillRoundedRectangle(r, radius);
 g.setColour(juce::Colours::white.withAlpha(.07f));
 g.drawRoundedRectangle(r.reduced(.5f), radius, 1.f);
 // Faint grid, like a measuring display.
 g.setColour(juce::Colours::white.withAlpha(.025f));
 for (float x = r.getX() + 16.f; x < r.getRight(); x += 16.f) g.drawVerticalLine(juce::roundToInt(x), r.getY() + 4.f, r.getBottom() - 4.f);
 for (float y = r.getY() + 16.f; y < r.getBottom(); y += 16.f) g.drawHorizontalLine(juce::roundToInt(y), r.getX() + 4.f, r.getRight() - 4.f);
}

void glowStroke(juce::Graphics& g, const juce::Path& p, juce::Colour c, float width) {
 g.setColour(c.withAlpha(.12f));
 g.strokePath(p, juce::PathStrokeType(width * 5.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
 g.setColour(c.withAlpha(.28f));
 g.strokePath(p, juce::PathStrokeType(width * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
 g.setColour(c.brighter(.25f));
 g.strokePath(p, juce::PathStrokeType(width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ---------------------------------------------------------------- look

Look::Look() {
 setColour(juce::PopupMenu::backgroundColourId, colour::card);
 setColour(juce::PopupMenu::textColourId, colour::ink);
 setColour(juce::PopupMenu::highlightedBackgroundColourId, colour::track);
 setColour(juce::PopupMenu::highlightedTextColourId, colour::ink);
 setColour(juce::ComboBox::textColourId, colour::ink);
 setColour(juce::ComboBox::arrowColourId, colour::muted);
 setColour(juce::TextButton::textColourOffId, colour::ink);
 setColour(juce::TextButton::textColourOnId, colour::ink);
 setColour(juce::ToggleButton::textColourId, colour::ink);
 setColour(juce::Label::textColourId, colour::muted);
 setColour(juce::TooltipWindow::backgroundColourId, colour::ink);
 setColour(juce::TooltipWindow::textColourId, colour::card);
}

juce::Typeface::Ptr Look::getTypefaceForFont(const juce::Font& f) {
 if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName()) return f.isBold() ? semibold() : medium();
 return LookAndFeel_V4::getTypefaceForFont(f);
}

void Look::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s) {
 const auto accent = s.findColour(juce::Slider::rotarySliderFillColourId);
 const float size = static_cast<float>(std::min(w, h));
 const auto centre = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)).getCentre();
 const float ring = size * .5f - 3.f, capR = ring * .74f;
 const float angle = start + pos * (end - start);
 // Track and value arc (bipolar ranges grow from the centre).
 juce::Path track;
 track.addCentredArc(centre.x, centre.y, ring, ring, 0.f, start, end, true);
 g.setColour(colour::track);
 g.strokePath(track, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
 float from = start;
 if (s.getMinimum() < 0. && s.getMaximum() > 0.) from = start + static_cast<float>(s.valueToProportionOfLength(0.)) * (end - start);
 if (std::abs(angle - from) > .01f) {
  juce::Path value;
  value.addCentredArc(centre.x, centre.y, ring, ring, 0.f, std::min(from, angle), std::max(from, angle), true);
  g.setColour(accent.withAlpha(.25f));
  g.strokePath(value, juce::PathStrokeType(6.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
  g.setColour(accent);
  g.strokePath(value, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
 }
 // The white cap, raised.
 juce::Path cap;
 cap.addEllipse(juce::Rectangle<float>(capR * 2.f, capR * 2.f).withCentre(centre));
 juce::DropShadow(colour::shadow.withAlpha(.55f), juce::roundToInt(capR * .45f), {juce::roundToInt(capR * .12f), juce::roundToInt(capR * .2f)}).drawForPath(g, cap);
 juce::DropShadow(juce::Colours::white, juce::roundToInt(capR * .3f), {-juce::roundToInt(capR * .1f), -juce::roundToInt(capR * .12f)}).drawForPath(g, cap);
 g.setGradientFill(juce::ColourGradient(juce::Colours::white, centre.x - capR, centre.y - capR, juce::Colour(0xffe7ebf0), centre.x + capR, centre.y + capR, false));
 g.fillPath(cap);
 g.setColour(juce::Colours::white);
 g.strokePath(cap, juce::PathStrokeType(1.f));
 // Indicator.
 const juce::Point<float> dot(centre.x + std::sin(angle) * capR * .68f, centre.y - std::cos(angle) * capR * .68f);
 g.setColour(colour::ink.withAlpha(.85f));
 g.fillEllipse(juce::Rectangle<float>(4.2f, 4.2f).withCentre(dot));
}

void Look::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) {
 const auto r = juce::Rectangle<float>(0.f, 0.f, static_cast<float>(w), static_cast<float>(h)).reduced(1.f);
 const float radius = r.getHeight() * .5f;
 g.setGradientFill(juce::ColourGradient(colour::track.darker(.06f), 0.f, r.getY(), colour::card, 0.f, r.getBottom(), false));
 g.fillRoundedRectangle(r, radius);
 g.setColour(juce::Colours::white.withAlpha(.9f));
 g.drawRoundedRectangle(r.translated(0.f, .5f), radius, 1.f);
 juce::Path chevron;
 const float cx = r.getRight() - radius - 2.f, cy = r.getCentreY();
 chevron.startNewSubPath(cx - 3.5f, cy - 1.5f);
 chevron.lineTo(cx, cy + 2.f);
 chevron.lineTo(cx + 3.5f, cy - 1.5f);
 g.setColour(box.findColour(juce::ComboBox::arrowColourId));
 g.strokePath(chevron, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
void Look::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
 label.setBounds(10, 0, box.getWidth() - 28, box.getHeight());
 label.setFont(getComboBoxFont(box));
 label.setJustificationType(juce::Justification::centredLeft);
}
juce::Font Look::getComboBoxFont(juce::ComboBox&) { return uiFont(12.f); }
juce::Font Look::getPopupMenuFont() { return uiFont(13.f); }
void Look::drawPopupMenuBackground(juce::Graphics& g, int w, int h) {
 g.fillAll(colour::card);
 g.setColour(colour::faint);
 g.drawRect(0, 0, w, h);
}

void Look::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool, bool) {
 const auto accent = b.findColour(juce::ToggleButton::tickColourId);
 const float h = 18.f, w = 34.f;
 const auto r = juce::Rectangle<float>(w, h).withCentre({static_cast<float>(b.getWidth()) * .5f, static_cast<float>(b.getHeight()) * .5f});
 const bool on = b.getToggleState();
 g.setColour(on ? accent : colour::track.darker(.05f));
 g.fillRoundedRectangle(r, h * .5f);
 const auto thumb = juce::Rectangle<float>(h - 4.f, h - 4.f).withCentre({on ? r.getRight() - h * .5f : r.getX() + h * .5f, r.getCentreY()});
 juce::Path t;
 t.addEllipse(thumb);
 juce::DropShadow(colour::shadow.withAlpha(.6f), 4, {0, 1}).drawForPath(g, t);
 g.setColour(juce::Colours::white);
 g.fillPath(t);
}

void Look::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down) {
 const auto r = b.getLocalBounds().toFloat().reduced(3.f);
 const float radius = r.getHeight() * .5f;
 if (!down) {
  juce::Path p;
  p.addRoundedRectangle(r, radius);
  juce::DropShadow(colour::shadow.withAlpha(.45f), 8, {2, 3}).drawForPath(g, p);
  juce::DropShadow(juce::Colours::white, 6, {-2, -2}).drawForPath(g, p);
 }
 g.setGradientFill(juce::ColourGradient(down ? colour::track : juce::Colours::white, 0.f, r.getY(),
                                        down ? colour::card : colour::card.darker(.03f), 0.f, r.getBottom(), false));
 g.fillRoundedRectangle(r, radius);
 if (highlighted && !down) { g.setColour(colour::faint.withAlpha(.6f)); g.drawRoundedRectangle(r, radius, 1.f); }
}
juce::Font Look::getTextButtonFont(juce::TextButton&, int h) { return uiFont(std::min(12.f, static_cast<float>(h) * .5f), true); }

// ---------------------------------------------------------------- knob, screen

Knob::Knob(juce::Colour accent) {
 setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
 setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
 setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
 setColour(juce::Slider::rotarySliderFillColourId, accent);
 setMouseDragSensitivity(220);
 setVelocityBasedMode(false);
}
void Knob::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
 if (!isEnabled() || std::abs(wheel.deltaY) < 1e-6f) return;
 const double step = static_cast<double>(wheel.deltaY) * (wheel.isReversed ? -1. : 1.) * (e.mods.isShiftDown() ? .04 : .25);
 const double proportion = juce::jlimit(0., 1., valueToProportionOfLength(getValue()) + step);
 setValue(proportionOfLengthToValue(proportion), juce::sendNotificationSync);
}

void Screen::paint(juce::Graphics& g) {
 const auto r = getLocalBounds().toFloat().reduced(2.f);
 paintScreen(g, r);
 if (draw) {
  juce::Graphics::ScopedSaveState save(g);
  g.reduceClipRegion(r.reduced(1.f).toNearestInt());
  draw(g, r.reduced(10.f, 8.f), page);
 }
}

ScreenChoice::ScreenChoice(juce::AudioProcessorValueTreeState& apvts, const ParamSpec& spec, juce::String captionText, juce::Colour accentColour)
 : param(apvts.getParameter(spec.id)),
   attachment(*param, [this](float v) { index = juce::roundToInt(v); repaint(); }, nullptr),
   items(juce::StringArray::fromTokens(spec.choices, "|", "")), caption(std::move(captionText)), accent(accentColour) {
 attachment.sendInitialUpdate();
 setMouseCursor(juce::MouseCursor::PointingHandCursor);
}
void ScreenChoice::paint(juce::Graphics& g) {
 const auto r = getLocalBounds().toFloat();
 g.setColour(colour::screenMuted);
 g.setFont(monoFont(9.f));
 g.drawText(caption, r.withHeight(11.f), juce::Justification::centred);
 const auto row = r.withTrimmedTop(11.f);
 g.setColour(accent.withAlpha(.85f));
 g.setFont(uiFont(14.f, true));
 g.drawText(juce::String::fromUTF8("\xe2\x80\xb9"), row.withWidth(16.f), juce::Justification::centred);
 g.drawText(juce::String::fromUTF8("\xe2\x80\xba"), row.withTrimmedLeft(row.getWidth() - 16.f), juce::Justification::centred);
 g.setColour(colour::screenInk);
 g.setFont(uiFont(12.5f, true));
 g.drawFittedText(items[juce::jlimit(0, items.size() - 1, index)].toUpperCase(), row.reduced(16.f, 0.f).toNearestInt(), juce::Justification::centred, 1, .7f);
}
void ScreenChoice::step(int delta) {
 const int n = items.size();
 attachment.setValueAsCompleteGesture(static_cast<float>((index + delta + n) % n));
}
void ScreenChoice::mouseUp(const juce::MouseEvent& e) {
 if (!getLocalBounds().contains(e.getPosition())) return;
 if (e.x < 20) { step(-1); return; }
 if (e.x > getWidth() - 20) { step(1); return; }
 juce::PopupMenu menu;
 for (int i = 0; i < items.size(); ++i) menu.addItem(i + 1, items[i], true, i == index);
 juce::Component::SafePointer<ScreenChoice> safe(this);
 menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [safe](int chosen) {
  if (safe != nullptr && chosen > 0) safe->attachment.setValueAsCompleteGesture(static_cast<float>(chosen - 1));
 });
}

// ---------------------------------------------------------------- module

std::vector<juce::Component*> Module::Cell::parts() const {
 return {name.get(), value.get(), knob.get(), combo.get(), toggle.get()};
}

Module::Module(juce::AudioProcessorValueTreeState& apvts, const ParamSpec* table, const char* const* shortNames,
               juce::String titleText, juce::Colour accentColour, std::vector<Page> pageList, int screenH,
               std::vector<int> screenChoices)
 : accent(accentColour), specs(table), title(std::move(titleText)), pages(std::move(pageList)), screenHeight(screenH) {
 if (screenHeight > 0) addAndMakeVisible(screen);
 for (int p : screenChoices) {
  choices.push_back(std::make_unique<ScreenChoice>(apvts, specs[p], juce::String::fromUTF8(shortNames[p]), accent));
  addAndMakeVisible(*choices.back());
 }
 for (int pg = 0; pg < static_cast<int>(pages.size()); ++pg) {
  for (int param : pages[static_cast<std::size_t>(pg)].params) {
   const auto& spec = specs[param];
   Cell c;
   c.param = param;
   c.page = pg;
   const auto& bigs = pages[static_cast<std::size_t>(pg)].big;
   c.big = std::find(bigs.begin(), bigs.end(), param) != bigs.end();
   c.name = std::make_unique<juce::Label>();
   c.name->setText(juce::String::fromUTF8(shortNames[param]).toUpperCase(), juce::dontSendNotification);
   c.name->setFont(uiFont(10.f, true).withExtraKerningFactor(.05f));
   c.name->setColour(juce::Label::textColourId, colour::muted);
   c.name->setJustificationType(juce::Justification::centred);
   c.name->setTooltip(spec.name);
   c.name->setInterceptsMouseClicks(false, false);
   addChildComponent(*c.name);
   if (spec.kind == Kind::Choice) {
    c.combo = std::make_unique<juce::ComboBox>();
    c.combo->addItemList(juce::StringArray::fromTokens(spec.choices, "|", ""), 1);
    addChildComponent(*c.combo);
    c.ca = std::make_unique<CA>(apvts, spec.id, *c.combo);
   } else if (spec.kind == Kind::Bool) {
    c.toggle = std::make_unique<juce::ToggleButton>();
    c.toggle->setColour(juce::ToggleButton::tickColourId, accent);
    addChildComponent(*c.toggle);
    c.ba = std::make_unique<BA>(apvts, spec.id, *c.toggle);
   } else {
    c.knob = std::make_unique<Knob>(accent);
    c.knob->setTooltip(spec.name);
    addChildComponent(*c.knob);
    c.sa = std::make_unique<SA>(apvts, spec.id, *c.knob);
    c.knob->setDoubleClickReturnValue(true, static_cast<double>(spec.def));
    c.value = std::make_unique<juce::Label>();
    c.value->setFont(monoFont(11.5f));
    c.value->setColour(juce::Label::textColourId, colour::ink);
    c.value->setJustificationType(juce::Justification::centred);
    c.value->setInterceptsMouseClicks(false, false);
    addChildComponent(*c.value);
    auto* knob = c.knob.get();
    auto* value = c.value.get();
    knob->onValueChange = [knob, value, &spec] { value->setText(abismo_ui::formatValue(spec, knob->getValue()), juce::dontSendNotification); };
    knob->onValueChange();
   }
   cells.push_back(std::move(c));
  }
 }
 setPage(0);
}

void Module::addHeaderComponent(juce::Component& c, int width) {
 headerParts.emplace_back(&c, width);
 addAndMakeVisible(c);
 resized();
}

void Module::setPage(int p) {
 current = juce::jlimit(0, std::max(0, static_cast<int>(pages.size()) - 1), p);
 screen.page = current;
 for (auto& c : cells) for (auto* part : c.parts()) if (part) part->setVisible(c.page == current);
 resized();
 repaint();
}

void Module::dimInactive(const std::function<bool(int)>& isActive) {
 auto apply = [](juce::Component* c, float alpha) { if (c && std::abs(c->getAlpha() - alpha) > .01f) c->setAlpha(alpha); };
 for (auto& c : cells) {
  const float alpha = isActive(c.param) ? 1.f : .3f;
  for (auto* part : c.parts()) apply(part, alpha);
 }
}

juce::Rectangle<int> Module::tabArea(int index) const {
 if (pages.size() < 2) return {};
 const auto inner = getLocalBounds().reduced(static_cast<int>(cardMargin)).reduced(16, 12);
 int x = inner.getX() + 22 + juce::roundToInt(juce::GlyphArrangement::getStringWidth(uiFont(12.5f, true), title)) + 16;
 for (int i = 0; i < static_cast<int>(pages.size()); ++i) {
  const int w = juce::roundToInt(juce::GlyphArrangement::getStringWidth(uiFont(10.f, true), pages[static_cast<std::size_t>(i)].name.toUpperCase())) + 16;
  if (i == index) return {x, inner.getY(), w, 20};
  x += w;
 }
 return {};
}

void Module::paint(juce::Graphics& g) {
 const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
 const int w = juce::roundToInt(static_cast<float>(getWidth()) * scale), h = juce::roundToInt(static_cast<float>(getHeight()) * scale);
 if (cardCache.getWidth() != w || cardCache.getHeight() != h) {
  cardCache = juce::Image(juce::Image::ARGB, std::max(1, w), std::max(1, h), true);
  juce::Graphics cg(cardCache);
  cg.addTransform(juce::AffineTransform::scale(scale));
  paintCard(cg, getLocalBounds().toFloat().reduced(cardMargin));
 }
 g.drawImageTransformed(cardCache, juce::AffineTransform::scale(1.f / scale));
 const auto inner = getLocalBounds().reduced(static_cast<int>(cardMargin)).reduced(16, 12);
 // Title with the module's accent.
 g.setColour(accent);
 g.fillEllipse(static_cast<float>(inner.getX()), static_cast<float>(inner.getY()) + 6.f, 8.f, 8.f);
 g.setColour(colour::ink);
 g.setFont(uiFont(12.5f, true).withExtraKerningFactor(.05f));
 g.drawText(title, inner.getX() + 16, inner.getY(), 400, 20, juce::Justification::centredLeft);
 // Page tabs.
 for (int i = 0; i < static_cast<int>(pages.size()) && pages.size() > 1; ++i) {
  const auto t = tabArea(i);
  const bool on = i == current;
  g.setColour(on ? colour::ink : colour::muted.withAlpha(.8f));
  g.setFont(uiFont(10.f, true).withExtraKerningFactor(.05f));
  g.drawText(pages[static_cast<std::size_t>(i)].name.toUpperCase(), t, juce::Justification::centred);
  if (on) { g.setColour(accent); g.fillRoundedRectangle(static_cast<float>(t.getCentreX()) - 7.f, static_cast<float>(t.getBottom()), 14.f, 2.5f, 1.25f); }
 }
}

void Module::mouseUp(const juce::MouseEvent& e) {
 for (int i = 0; i < static_cast<int>(pages.size()); ++i)
  if (tabArea(i).expanded(2, 4).contains(e.getPosition())) { setPage(i); return; }
}

void Module::resized() {
 cardCache = {};
 auto inner = getLocalBounds().reduced(static_cast<int>(cardMargin)).reduced(16, 12);
 auto header = inner.removeFromTop(22);
 for (auto& [comp, width] : headerParts) comp->setBounds(header.removeFromRight(width).withSizeKeepingCentre(width, 26));
 inner.removeFromTop(8);
 if (screenHeight > 0) {
  auto s = inner.removeFromTop(screenHeight);
  screen.setBounds(s);
  if (!choices.empty()) {
   auto strip = s.removeFromBottom(34).reduced(10, 3);
   const int cw = strip.getWidth() / static_cast<int>(choices.size());
   for (auto& c : choices) c->setBounds(strip.removeFromLeft(cw).reduced(4, 0));
  }
  inner.removeFromTop(10);
 }
 // Controls of the current page: one row when they fit, else two; big knobs take more room.
 std::vector<Cell*> visible;
 for (auto& c : cells) if (c.page == current) visible.push_back(&c);
 if (visible.empty()) return;
 auto weight = [this](const Cell* c) { return c->big ? 1.6f : specs[c->param].kind == Kind::Choice ? 1.7f : 1.f; };
 // One row or two: whichever gives the bigger knobs.
 auto knobFor = [&](int rows) {
  const std::size_t per = (visible.size() + static_cast<std::size_t>(rows) - 1) / static_cast<std::size_t>(rows);
  float widest = 0.f;
  for (std::size_t from = 0; from < visible.size(); from += per) {
   float sum = 0.f;
   for (std::size_t i = from; i < std::min(visible.size(), from + per); ++i) sum += weight(visible[i]);
   widest = std::max(widest, sum);
  }
  return std::min(static_cast<float>(inner.getWidth()) / widest - 8.f, static_cast<float>(inner.getHeight()) / static_cast<float>(rows) - 31.f);
 };
 const int rows = visible.size() > 3 && knobFor(2) > knobFor(1) * 1.15f ? 2 : 1;
 const std::size_t perRow = (visible.size() + static_cast<std::size_t>(rows) - 1) / static_cast<std::size_t>(rows);
 const int rowH = inner.getHeight() / rows;
 for (int row = 0; row < rows; ++row) {
  auto line = inner.removeFromTop(rowH);
  const std::size_t from = static_cast<std::size_t>(row) * perRow, to = std::min(visible.size(), from + perRow);
  float sum = 0.f;
  for (std::size_t i = from; i < to; ++i) sum += weight(visible[i]);
  const float unit = static_cast<float>(line.getWidth()) / std::max(1.f, sum);
  for (std::size_t i = from; i < to; ++i) {
   auto* c = visible[i];
   auto cell = line.removeFromLeft(juce::roundToInt(unit * weight(c)));
   // Name, control and value stay together, centred in a tall cell.
   const int size = std::min({cell.getWidth() - 6, cell.getHeight() - 31, c->big ? 92 : 66});
   const int controlH = c->knob ? size : 28;
   auto group = cell.withSizeKeepingCentre(cell.getWidth(), std::min(cell.getHeight(), 15 + controlH + 6 + 16));
   c->name->setBounds(group.removeFromTop(15));
   if (c->knob) {
    c->value->setBounds(group.removeFromBottom(16));
    c->knob->setBounds(group.withSizeKeepingCentre(size, size));
   } else if (c->combo) {
    c->combo->setBounds(group.withTrimmedBottom(16).withSizeKeepingCentre(std::min(cell.getWidth() - 8, 150), 28));
   } else if (c->toggle) {
    c->toggle->setBounds(group.withTrimmedBottom(16).withSizeKeepingCentre(48, 28));
   }
  }
 }
}

// ---------------------------------------------------------------- meter, preset bar, body

void Meter::setLevel(float peak) {
 const float db = juce::jlimit(-60.f, 0.f, juce::Decibels::gainToDecibels(peak, -60.f));
 const float v = (db + 60.f) / 60.f;
 if (std::abs(v - shown) > .003f) { shown = v; repaint(); }
}
void Meter::paint(juce::Graphics& g) {
 const auto r = getLocalBounds().toFloat().reduced(2.f);
 g.setColour(colour::track.darker(.06f));
 g.fillRoundedRectangle(r, r.getWidth() * .5f);
 const auto fill = r.reduced(3.f).withTrimmedTop(r.reduced(3.f).getHeight() * (1.f - shown));
 juce::ColourGradient grad(colour::red, 0.f, r.getY(), colour::green, 0.f, r.getBottom(), false);
 grad.addColour(.25, colour::amber);
 g.setGradientFill(grad);
 g.fillRoundedRectangle(fill, (r.getWidth() - 6.f) * .5f);
}

Master::Master(juce::AudioProcessorValueTreeState& apvts, const ParamSpec& out, const ParamSpec* sec, juce::String secName, juce::Colour accent)
 : output(accent), second(accent), hasSecond(sec != nullptr), secondName(std::move(secName)) {
 auto m = std::make_unique<Meter>();
 meterPtr = m.get();
 meter = std::move(m);
 addAndMakeVisible(*meter);
 addAndMakeVisible(output);
 outAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, out.id, output);
 output.setDoubleClickReturnValue(true, static_cast<double>(out.def));
 for (auto* l : {&outValue, &secondValue}) {
  l->setFont(monoFont(12.f));
  l->setColour(juce::Label::textColourId, colour::ink);
  l->setJustificationType(juce::Justification::centred);
  l->setInterceptsMouseClicks(false, false);
 }
 addAndMakeVisible(outValue);
 output.onValueChange = [this, &out] { outValue.setText(abismo_ui::formatValue(out, output.getValue()), juce::dontSendNotification); };
 output.onValueChange();
 if (hasSecond) {
  addAndMakeVisible(second);
  addAndMakeVisible(secondValue);
  secondAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, sec->id, second);
  second.setDoubleClickReturnValue(true, static_cast<double>(sec->def));
  second.onValueChange = [this, sec] { secondValue.setText(abismo_ui::formatValue(*sec, second.getValue()), juce::dontSendNotification); };
  second.onValueChange();
 }
}
void Master::setReadout(float peak, juce::String line) {
 meterPtr->setLevel(peak);
 const float db = juce::Decibels::gainToDecibels(peak, -90.f);
 if (std::abs(db - peakDb) > .2f || line != readout) { peakDb = db; readout = line; repaint(); }
}
void Master::paint(juce::Graphics& g) {
 const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
 const int w = juce::roundToInt(static_cast<float>(getWidth()) * scale), h = juce::roundToInt(static_cast<float>(getHeight()) * scale);
 if (cardCache.getWidth() != w || cardCache.getHeight() != h) {
  cardCache = juce::Image(juce::Image::ARGB, std::max(1, w), std::max(1, h), true);
  juce::Graphics cg(cardCache);
  cg.addTransform(juce::AffineTransform::scale(scale));
  paintCard(cg, getLocalBounds().toFloat().reduced(cardMargin));
 }
 g.drawImageTransformed(cardCache, juce::AffineTransform::scale(1.f / scale));
 const auto inner = getLocalBounds().reduced(static_cast<int>(cardMargin)).reduced(14, 12);
 g.setColour(colour::ink);
 g.setFont(uiFont(12.5f, true).withExtraKerningFactor(.05f));
 g.drawText("MASTER", inner.withHeight(20), juce::Justification::centred);
 g.setColour(colour::muted);
 g.setFont(uiFont(9.5f, true).withExtraKerningFactor(.06f));
 g.drawText(juce::String::fromUTF8("SA\xc3\x8d" "DA"), output.getBounds().translated(0, -16).withHeight(14), juce::Justification::centred);
 if (hasSecond) g.drawText(secondName, second.getBounds().translated(0, -16).withHeight(14), juce::Justification::centred);
 g.setFont(monoFont(10.5f));
 g.setColour(peakDb > -1.f ? colour::red : colour::muted);
 g.drawText((peakDb <= -89.f ? juce::String("-inf") : juce::String(peakDb, 1)) + " dBFS", inner.withTrimmedTop(inner.getHeight() - 34).withHeight(16), juce::Justification::centred);
 g.setColour(colour::muted);
 g.drawText(readout, inner.withTrimmedTop(inner.getHeight() - 16), juce::Justification::centred);
}
void Master::resized() {
 auto inner = getLocalBounds().reduced(static_cast<int>(cardMargin)).reduced(14, 12);
 inner.removeFromTop(40);
 inner.removeFromBottom(40);
 const int knob = std::min(inner.getWidth(), 112);
 output.setBounds(inner.removeFromTop(knob).withSizeKeepingCentre(knob, knob));
 outValue.setBounds(inner.removeFromTop(18));
 if (hasSecond) {
  inner.removeFromTop(24);
  second.setBounds(inner.removeFromTop(64).withSizeKeepingCentre(64, 64));
  secondValue.setBounds(inner.removeFromTop(16));
 }
 inner.removeFromTop(16);
 meter->setBounds(inner.withSizeKeepingCentre(16, inner.getHeight()));
}

void PresetBar::paint(juce::Graphics& g) {
 const auto r = getLocalBounds().toFloat().reduced(8.f);
 juce::Path p;
 p.addRoundedRectangle(r, r.getHeight() * .5f);
 juce::DropShadow(colour::shadow.withAlpha(.4f), 12, {3, 4}).drawForPath(g, p);
 juce::DropShadow(juce::Colours::white, 8, {-3, -3}).drawForPath(g, p);
 g.setGradientFill(juce::ColourGradient(juce::Colours::white, 0.f, r.getY(), colour::card, 0.f, r.getBottom(), false));
 g.fillPath(p);
 const int count = getCount ? getCount() : 0, current = getCurrent ? getCurrent() : 0;
 g.setColour(colour::ink.withAlpha(.7f));
 g.setFont(uiFont(30.f));
 g.drawText(juce::String::fromUTF8("\xe2\x80\xb9"), r.withWidth(44.f).translated(6.f, -2.f), juce::Justification::centred);
 g.drawText(juce::String::fromUTF8("\xe2\x80\xba"), r.withTrimmedLeft(r.getWidth() - 44.f).translated(-6.f, -2.f), juce::Justification::centred);
 const auto mid = r.reduced(40.f, 0.f);
 g.setFont(monoFont(10.5f));
 g.setColour(colour::muted);
 g.drawText(juce::String(current + 1).paddedLeft('0', 2) + " / " + juce::String(count), mid.withHeight(mid.getHeight() * .42f).translated(0.f, 3.f), juce::Justification::centred);
 g.setColour(colour::ink);
 g.setFont(uiFont(16.f, true));
 g.drawFittedText(getName ? getName(current) : juce::String(), mid.withTrimmedTop(mid.getHeight() * .36f).toNearestInt(), juce::Justification::centred, 1, .8f);
}
void PresetBar::mouseUp(const juce::MouseEvent& e) {
 if (!getCount || !select) return;
 const int count = getCount(), current = getCurrent();
 if (count <= 0) return;
 if (e.x < 48) { select((current + count - 1) % count); repaint(); return; }
 if (e.x > getWidth() - 48) { select((current + 1) % count); repaint(); return; }
 juce::PopupMenu menu;
 for (int i = 0; i < count; ++i) menu.addItem(i + 1, getName(i), true, i == current);
 juce::Component::SafePointer<PresetBar> safe(this);
 menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMaximumNumColumns(2), [safe](int chosen) {
  if (safe != nullptr && chosen > 0) { safe->select(chosen - 1); safe->repaint(); }
 });
}

void Body::paint(juce::Graphics& g) {
 const auto r = getLocalBounds().toFloat();
 g.setGradientFill(juce::ColourGradient(colour::bodyTop, 0.f, 0.f, colour::bodyBottom, 0.f, r.getBottom(), false));
 g.fillAll();
 g.setColour(colour::muted);
 g.setFont(uiFont(10.5f, true).withExtraKerningFactor(.3f));
 g.drawText("INVERSO", 34, 16, 300, 14, juce::Justification::centredLeft);
 g.setColour(colour::ink);
 g.setFont(uiFont(27.f, true).withExtraKerningFactor(.08f));
 g.drawText(title, 32, 28, 520, 36, juce::Justification::centredLeft);
 g.setColour(colour::muted);
 g.setFont(uiFont(10.5f));
 g.drawText(subtitle, 34, 62, 700, 16, juce::Justification::centredLeft);
}

} // namespace soft
