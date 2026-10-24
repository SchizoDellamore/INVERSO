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
juce::Typeface::Ptr oswald() { static auto t = load(AbismoFonts::OswaldMedium_ttf, AbismoFonts::OswaldMedium_ttfSize); return t; }
juce::Typeface::Ptr oswaldBold() { static auto t = load(AbismoFonts::OswaldSemiBold_ttf, AbismoFonts::OswaldSemiBold_ttfSize); return t; }
juce::Typeface::Ptr bebas() { static auto t = load(AbismoFonts::BebasNeueRegular_ttf, AbismoFonts::BebasNeueRegular_ttfSize); return t; }
juce::Typeface::Ptr courier() { static auto t = load(AbismoFonts::CourierPrimeBold_ttf, AbismoFonts::CourierPrimeBold_ttfSize); return t; }
// Screws at the corners of a vintage panel.
void screw(juce::Graphics& g, juce::Point<float> c, float r, float angle) {
 g.setColour(juce::Colour(0x55000000));
 g.fillEllipse(juce::Rectangle<float>(r * 2.f, r * 2.f).withCentre(c.translated(.6f, .9f)));
 g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe6e1d6), c.x - r, c.y - r, juce::Colour(0xff8f877a), c.x + r, c.y + r, false));
 g.fillEllipse(juce::Rectangle<float>(r * 2.f, r * 2.f).withCentre(c));
 g.setColour(juce::Colour(0xff5a5348));
 const float dx = std::cos(angle) * r * .75f, dy = std::sin(angle) * r * .75f;
 g.drawLine(c.x - dx, c.y - dy, c.x + dx, c.y + dy, 1.3f);
}
constexpr float cardMargin = 10.f;
}

juce::Font uiFont(float height, bool bold) {
 if (vintage) return juce::Font(juce::FontOptions(bold ? oswaldBold() : oswald()).withHeight(height * 1.12f));
 return juce::Font(juce::FontOptions(bold ? semibold() : medium()).withHeight(height));
}
juce::Font monoFont(float height) {
 if (vintage) return juce::Font(juce::FontOptions(courier()).withHeight(height * 1.08f));
 return juce::Font(juce::FontOptions(mono()).withHeight(height));
}
juce::Font titleFont(float height) {
 if (vintage) return juce::Font(juce::FontOptions(bebas()).withHeight(height * 1.35f)).withExtraKerningFactor(.06f);
 return uiFont(height, true).withExtraKerningFactor(.05f);
}

void useVintage() {
 vintage = true;
 namespace c = colour;
 c::bodyTop = juce::Colour(0xff5a3520); c::bodyBottom = juce::Colour(0xff3a2113);
 c::card = juce::Colour(0xffe8dcc2); c::cardEdge = juce::Colour(0xfff6eedb);
 c::shadow = juce::Colour(0xff1a0e06);
 c::ink = juce::Colour(0xff2a221c); c::muted = juce::Colour(0xff6e5f4c); c::faint = juce::Colour(0xffb3a487);
 c::track = juce::Colour(0xffcdbf9f);
 c::screenTop = juce::Colour(0xff17200f); c::screenBottom = juce::Colour(0xff0b1007);
 c::screenInk = juce::Colour(0xffd9f7b5); c::screenMuted = juce::Colour(0xff7c9a64);
 c::amber = juce::Colour(0xffeaa23a); c::cyan = juce::Colour(0xff74cfc6); c::violet = juce::Colour(0xffb39be0); c::red = juce::Colour(0xffe2603f);
 c::orange = juce::Colour(0xffe9853c); c::green = juce::Colour(0xff98d474); c::pink = juce::Colour(0xffe58fa0); c::blue = juce::Colour(0xff7fb0e0);
 c::blood = juce::Colour(0xffc0392b);
}

// ---------------------------------------------------------------- drawing helpers

void paintCard(juce::Graphics& g, juce::Rectangle<float> r, float radius) {
 if (vintage) {
  // Cream enamel plate on the wood: dark drop shadow, an engraved border line and four screws.
  juce::Path p;
  p.addRoundedRectangle(r, 5.f);
  juce::DropShadow(juce::Colour(0xcc000000), 14, {3, 6}).drawForPath(g, p);
  g.setGradientFill(juce::ColourGradient(colour::cardEdge, r.getX(), r.getY(), colour::card.darker(.08f), r.getRight(), r.getBottom(), false));
  g.fillPath(p);
  // Faint enamel speckle, seeded by the size so a plate always looks the same.
  juce::Random rnd(static_cast<juce::int64>(r.getWidth() * 131.f + r.getHeight()));
  for (int i = 0; i < static_cast<int>(r.getWidth() * r.getHeight() / 90.f); ++i) {
   g.setColour(juce::Colour(0xff6e5f4c).withAlpha(.04f + .05f * rnd.nextFloat()));
   g.fillRect(r.getX() + rnd.nextFloat() * r.getWidth(), r.getY() + rnd.nextFloat() * r.getHeight(), 1.2f, 1.2f);
  }
  g.setColour(juce::Colour(0x66ffffff));
  g.drawRoundedRectangle(r.reduced(1.f), 5.f, 1.f);
  g.setColour(colour::muted.withAlpha(.45f));
  g.drawRoundedRectangle(r.reduced(7.f), 3.f, 1.f);
  const float s = 4.2f;
  screw(g, {r.getX() + 4.f + s, r.getY() + 4.f + s}, s, .5f);
  screw(g, {r.getRight() - 4.f - s, r.getY() + 4.f + s}, s, 1.9f);
  screw(g, {r.getX() + 4.f + s, r.getBottom() - 4.f - s}, s, 2.8f);
  screw(g, {r.getRight() - 4.f - s, r.getBottom() - 4.f - s}, s, .9f);
  return;
 }
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
 if (vintage) {
  // A black bezel, then a phosphor tube: dark green glass, scanlines, a dimmer edge.
  g.setColour(juce::Colour(0xff1b1612));
  g.fillRoundedRectangle(r.expanded(3.f), 6.f);
  g.setColour(juce::Colour(0x33ffffff));
  g.drawRoundedRectangle(r.expanded(3.f).translated(0.f, .8f), 6.f, 1.f);
  g.setGradientFill(juce::ColourGradient(colour::screenTop.brighter(.12f), r.getCentreX(), r.getCentreY(), colour::screenBottom,
                                         r.getX(), r.getY(), true));
  g.fillRoundedRectangle(r, 8.f);
  g.setColour(juce::Colours::black.withAlpha(.18f));
  for (float y = r.getY() + 1.f; y < r.getBottom(); y += 3.f) g.drawHorizontalLine(juce::roundToInt(y), r.getX() + 2.f, r.getRight() - 2.f);
  g.setColour(colour::screenInk.withAlpha(.05f));
  for (float x = r.getX() + 20.f; x < r.getRight(); x += 20.f) g.drawVerticalLine(juce::roundToInt(x), r.getY() + 4.f, r.getBottom() - 4.f);
  for (float y = r.getY() + 20.f; y < r.getBottom(); y += 20.f) g.drawHorizontalLine(juce::roundToInt(y), r.getX() + 4.f, r.getRight() - 4.f);
  g.setColour(juce::Colours::white.withAlpha(.04f));
  g.fillRoundedRectangle(r.withHeight(r.getHeight() * .35f).reduced(6.f, 3.f), 8.f);
  return;
 }
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
 if (vintage) {
  // Printed scale on the panel, a thin value arc, a knurled bakelite skirt and
  // a glossy cap with a cream pointer line.
  const float outer = size * .5f - 1.f, skirt = outer * .74f, capR = skirt * .72f;
  const float angle = start + pos * (end - start);
  g.setColour(colour::ink.withAlpha(.75f));
  for (int i = 0; i <= 10; ++i) {
   const float a = start + (end - start) * static_cast<float>(i) / 10.f;
   const float r1 = outer * (i % 5 == 0 ? .84f : .89f), r2 = outer;
   g.drawLine(centre.x + std::sin(a) * r1, centre.y - std::cos(a) * r1, centre.x + std::sin(a) * r2, centre.y - std::cos(a) * r2, i % 5 == 0 ? 1.6f : 1.f);
  }
  float from = start;
  if (s.getMinimum() < 0. && s.getMaximum() > 0.) from = start + static_cast<float>(s.valueToProportionOfLength(0.)) * (end - start);
  if (std::abs(angle - from) > .01f) {
   juce::Path value;
   value.addCentredArc(centre.x, centre.y, outer * .79f, outer * .79f, 0.f, std::min(from, angle), std::max(from, angle), true);
   g.setColour(accent.darker(.25f));
   g.strokePath(value, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
  }
  juce::Path body;
  body.addEllipse(juce::Rectangle<float>(skirt * 2.f, skirt * 2.f).withCentre(centre));
  juce::DropShadow(juce::Colour(0x99000000), juce::roundToInt(skirt * .35f), {juce::roundToInt(skirt * .08f), juce::roundToInt(skirt * .16f)}).drawForPath(g, body);
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3a332d), centre.x, centre.y - skirt, juce::Colour(0xff0d0b09), centre.x, centre.y + skirt, false));
  g.fillPath(body);
  g.setColour(juce::Colour(0x22ffffff));
  for (int i = 0; i < 36; ++i) { // knurling, turning with the knob
   const float a = static_cast<float>(i) * juce::MathConstants<float>::twoPi / 36.f + angle;
   g.drawLine(centre.x + std::sin(a) * capR * 1.04f, centre.y - std::cos(a) * capR * 1.04f, centre.x + std::sin(a) * skirt * .97f, centre.y - std::cos(a) * skirt * .97f, 1.f);
  }
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4a423b), centre.x - capR * .5f, centre.y - capR, juce::Colour(0xff120f0c), centre.x + capR * .4f, centre.y + capR, false));
  g.fillEllipse(juce::Rectangle<float>(capR * 2.f, capR * 2.f).withCentre(centre));
  g.setColour(juce::Colours::white.withAlpha(.16f));
  g.fillEllipse(juce::Rectangle<float>(capR * 1.1f, capR * .55f).withCentre(centre.translated(-capR * .15f, -capR * .48f)));
  g.setColour(juce::Colour(0xffefe4c8));
  g.drawLine(centre.x + std::sin(angle) * capR * .2f, centre.y - std::cos(angle) * capR * .2f,
             centre.x + std::sin(angle) * skirt * .94f, centre.y - std::cos(angle) * skirt * .94f, 2.2f);
  return;
 }
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
 if (vintage) { // a black label plate
  g.setColour(juce::Colour(0xff1d1814));
  g.fillRoundedRectangle(r, 3.f);
  g.setColour(juce::Colour(0x44ffffff));
  g.drawRoundedRectangle(r.reduced(.5f), 3.f, 1.f);
  juce::Path tri;
  const float cx = r.getRight() - 12.f, cy = r.getCentreY();
  tri.addTriangle(cx - 4.f, cy - 2.f, cx + 4.f, cy - 2.f, cx, cy + 3.f);
  g.setColour(colour::amber);
  g.fillPath(tri);
  box.setColour(juce::ComboBox::textColourId, juce::Colour(0xffefe4c8));
  return;
 }
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
 if (vintage) { // a chrome bat switch and an indicator lamp
  const auto c = juce::Point<float>(static_cast<float>(b.getWidth()) * .5f - 8.f, static_cast<float>(b.getHeight()) * .5f);
  const bool on = b.getToggleState();
  g.setColour(juce::Colour(0xff8f877a));
  g.fillEllipse(juce::Rectangle<float>(14.f, 14.f).withCentre(c));
  g.setColour(juce::Colour(0xffd8d2c6));
  g.drawLine(c.x, c.y, c.x, c.y + (on ? -11.f : 11.f), 4.f);
  const auto lamp = juce::Rectangle<float>(9.f, 9.f).withCentre(c.translated(20.f, 0.f));
  if (on) { g.setColour(accent.withAlpha(.35f)); g.fillEllipse(lamp.expanded(4.f)); }
  g.setColour(on ? accent.brighter(.3f) : juce::Colour(0xff4a3f35));
  g.fillEllipse(lamp);
  return;
 }
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
 if (vintage) {
  g.setColour(juce::Colour(down ? 0xff0d0b09 : 0xff2a241f));
  g.fillRoundedRectangle(r, 3.f);
  g.setColour(juce::Colour(highlighted ? 0x66ffffff : 0x33ffffff));
  g.drawRoundedRectangle(r.reduced(.5f), 3.f, 1.f);
  b.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffefe4c8));
  return;
 }
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
void Knob::enableMidiLearn(juce::AudioProcessorValueTreeState& apvts, const juce::String& id) {
 learn = abismo_ui::MidiLearn::find(&apvts.processor);
 parameterIndex = learn != nullptr ? learn->indexOf(id) : -1;
 baseTooltip = getTooltip();
}
void Knob::mouseEnter(const juce::MouseEvent& e) {
 if (learn != nullptr && parameterIndex >= 0) {
  const int cc = learn->ccFor(parameterIndex);
  setTooltip(baseTooltip + (cc >= 0 ? "   (MIDI CC " + juce::String(cc) + ", right-click to change)" : juce::String("   (right-click: MIDI learn)")));
 }
 juce::Slider::mouseEnter(e);
}
void Knob::mouseDown(const juce::MouseEvent& e) {
 if (e.mods.isPopupMenu() && learn != nullptr && parameterIndex >= 0) {
  const int cc = learn->ccFor(parameterIndex);
  juce::PopupMenu menu;
  menu.addSectionHeader(baseTooltip.isNotEmpty() ? baseTooltip : juce::String("This knob"));
  menu.addItem(1, cc >= 0 ? "MIDI learn again (now CC " + juce::String(cc) + ")" : juce::String("MIDI learn: then turn a knob on your keyboard"));
  menu.addItem(2, "Forget MIDI", cc >= 0);
  if (learn->armedParameter() >= 0) menu.addItem(3, "Cancel MIDI learn");
  juce::Component::SafePointer<Knob> safe(this);
  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [safe](int chosen) {
   if (safe == nullptr || safe->learn == nullptr) return;
   if (chosen == 1) { safe->learn->arm(safe->parameterIndex); safe->startTimerHz(6); }
   if (chosen == 2) safe->learn->forget(safe->parameterIndex);
   if (chosen == 3) safe->learn->disarm();
   safe->repaint();
  });
  return;
 }
 juce::Slider::mouseDown(e);
}
void Knob::timerCallback() {
 // blinking while it waits for a CC; stops once the CC arrived (or learn was cancelled)
 blink = !blink;
 if (learn == nullptr || learn->armedParameter() != parameterIndex) { stopTimer(); blink = false; }
 repaint();
}
void Knob::paintOverChildren(juce::Graphics& g) {
 if (learn == nullptr || parameterIndex < 0) return;
 const auto r = getLocalBounds().toFloat();
 const auto accent = findColour(juce::Slider::rotarySliderFillColourId);
 if (learn->armedParameter() == parameterIndex) {
  if (blink) { g.setColour(accent); g.drawEllipse(r.reduced(1.5f), 2.5f); }
 } else if (learn->ccFor(parameterIndex) >= 0) {
  g.setColour(accent);
  g.fillEllipse(r.getRight() - 9.f, r.getY() + 2.f, 7.f, 7.f); // learned: a small dot
 }
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
    c.knob->enableMidiLearn(apvts, spec.id);
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
 int x = inner.getX() + 22 + juce::roundToInt(juce::GlyphArrangement::getStringWidth(titleFont(12.5f), title)) + 16;
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
 const auto lamp = juce::Rectangle<float>(static_cast<float>(inner.getX()), static_cast<float>(inner.getY()) + 6.f, 8.f, 8.f);
 if (vintage) { // a jewel pilot lamp
  g.setColour(accent.withAlpha(.3f)); g.fillEllipse(lamp.expanded(3.f));
  g.setColour(juce::Colour(0xff2a221c)); g.fillEllipse(lamp.expanded(1.2f));
 }
 g.setColour(accent);
 g.fillEllipse(lamp);
 g.setColour(colour::ink);
 g.setFont(titleFont(12.5f));
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
 if (vintage) { // the needle has weight: it rises and falls at its own pace
  const float target = juce::jlimit(-22.f, 4.f, db + 18.f);
  const float next = needle + (target > needle ? .45f : .18f) * (target - needle);
  if (std::abs(next - needle) > .02f) { needle = next; repaint(); }
  return;
 }
 const float v = (db + 60.f) / 60.f;
 if (std::abs(v - shown) > .003f) { shown = v; repaint(); }
}
void Meter::paint(juce::Graphics& g) {
 const auto r = getLocalBounds().toFloat().reduced(2.f);
 if (vintage) {
  g.setColour(juce::Colour(0xff1b1612));
  g.fillRoundedRectangle(r, 5.f);
  const auto face = r.reduced(5.f);
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff6e7b8), face.getCentreX(), face.getBottom(), juce::Colour(0xffd9be7a), face.getX(), face.getY(), true));
  g.fillRoundedRectangle(face, 3.f);
  const juce::Point<float> pivot(face.getCentreX(), face.getBottom() + face.getHeight() * .25f);
  const float radius = face.getHeight() * .95f;
  auto angleOf = [](float vu) { return juce::jmap(vu, -20.f, 3.f, -.85f, .85f); };
  auto at = [&](float a, float rr) { return juce::Point<float>(pivot.x + std::sin(a) * rr, pivot.y - std::cos(a) * rr); };
  juce::Path scale, hot;
  scale.addCentredArc(pivot.x, pivot.y, radius * .8f, radius * .8f, 0.f, angleOf(-20.f), angleOf(0.f), true);
  hot.addCentredArc(pivot.x, pivot.y, radius * .8f, radius * .8f, 0.f, angleOf(0.f), angleOf(3.f), true);
  g.setColour(juce::Colour(0xff2a221c));
  g.strokePath(scale, juce::PathStrokeType(1.4f));
  g.setColour(juce::Colour(0xffc0392b));
  g.strokePath(hot, juce::PathStrokeType(3.f));
  g.setFont(monoFont(8.5f));
  for (float vu : {-20.f, -10.f, -7.f, -5.f, -3.f, 0.f, 3.f}) {
   const float a = angleOf(vu);
   g.setColour(vu > 0.f ? juce::Colour(0xffc0392b) : juce::Colour(0xff2a221c));
   g.drawLine(juce::Line<float>(at(a, radius * .8f), at(a, radius * .88f)), 1.2f);
   g.drawText(juce::String(static_cast<int>(std::abs(vu))), juce::Rectangle<float>(20.f, 10.f).withCentre(at(a, radius * .97f)), juce::Justification::centred);
  }
  g.setColour(juce::Colour(0xff2a221c));
  g.setFont(titleFont(10.f));
  g.drawText("VU", face.withTrimmedTop(face.getHeight() * .55f), juce::Justification::centredTop);
  g.saveState();
  g.reduceClipRegion(face.toNearestInt());
  g.setColour(juce::Colour(0xff1b1612));
  g.drawLine(juce::Line<float>(pivot, at(angleOf(needle), radius * .9f)), 1.6f);
  g.restoreState();
  g.setColour(juce::Colours::white.withAlpha(.12f));
  g.fillRoundedRectangle(face.withHeight(face.getHeight() * .3f), 3.f);
  return;
 }
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
 output.setTooltip(out.name);
 output.enableMidiLearn(apvts, out.id);
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
  second.setTooltip(sec->name);
  second.enableMidiLearn(apvts, sec->id);
  second.onValueChange = [this, sec] { secondValue.setText(abismo_ui::formatValue(*sec, second.getValue()), juce::dontSendNotification); };
  second.onValueChange();
 }
}
void Master::setAccessory(std::unique_ptr<juce::Component> c, int height) {
 accessory = std::move(c);
 accessoryHeight = height;
 addAndMakeVisible(*accessory);
 resized();
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
 g.setFont(titleFont(12.5f));
 g.drawText("MASTER", inner.withHeight(20), juce::Justification::centred);
 g.setColour(colour::muted);
 g.setFont(uiFont(9.5f, true).withExtraKerningFactor(.06f));
 g.drawText("OUTPUT", output.getBounds().translated(0, -16).withHeight(14), juce::Justification::centred);
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
 if (accessory) { accessory->setBounds(inner.removeFromBottom(accessoryHeight)); inner.removeFromBottom(12); }
 if (vintage) meter->setBounds(inner.withSizeKeepingCentre(inner.getWidth(), std::min(inner.getHeight(), inner.getWidth() * 3 / 4)));
 else meter->setBounds(inner.withSizeKeepingCentre(16, inner.getHeight()));
}

KnobLearn::KnobLearn(juce::AudioProcessorValueTreeState& apvts, const ParamSpec& spec, std::atomic<bool>& armedFlag, std::atomic<int>& learnedCC)
 : param(apvts.getParameter(spec.id)),
   attachment(*param, [this](float v) { cc = juce::roundToInt(v); refresh(); }, nullptr),
   armed(armedFlag), learned(learnedCC) {
 addAndMakeVisible(learnButton);
 addAndMakeVisible(clearButton);
 learnButton.setTooltip("Click, then turn a knob on your keyboard: it will work as aftertouch");
 clearButton.setTooltip("Stop using a knob as aftertouch");
 learnButton.onClick = [this] { learned.store(-1); armed.store(!armed.load()); refresh(); };
 clearButton.onClick = [this] { armed.store(false); attachment.setValueAsCompleteGesture(0.f); };
 attachment.sendInitialUpdate();
 startTimerHz(10);
}
KnobLearn::~KnobLearn() { armed.store(false); }
void KnobLearn::timerCallback() {
 const int got = learned.exchange(-1);
 if (got > 0) { armed.store(false); attachment.setValueAsCompleteGesture(static_cast<float>(got)); }
 refresh();
}
void KnobLearn::refresh() {
 const juce::String text = armed.load() ? juce::String("TURN A KNOB...") : cc > 0 ? "KNOB CC " + juce::String(cc) : juce::String("LEARN");
 if (learnButton.getButtonText() != text) learnButton.setButtonText(text);
 clearButton.setVisible(cc > 0 && !armed.load());
}
void KnobLearn::resized() {
 auto r = getLocalBounds();
 r.removeFromTop(16);
 auto row = r.removeFromTop(32);
 clearButton.setBounds(row.removeFromRight(30));
 learnButton.setBounds(row);
}
void KnobLearn::paint(juce::Graphics& g) {
 g.setColour(colour::muted);
 g.setFont(uiFont(9.5f, true).withExtraKerningFactor(.06f));
 g.drawText("AFTERTOUCH KNOB", getLocalBounds().removeFromTop(14), juce::Justification::centred);
}

void PresetBar::enableUserPresets(juce::AudioProcessorValueTreeState& apvts, const juce::String& plugin) {
 userApvts = &apvts;
 userPlugin = plugin;
 repaint();
}

void PresetBar::paint(juce::Graphics& g) {
 auto all = getLocalBounds().toFloat().reduced(8.f);
 juce::Rectangle<float> save;
 if (saveWidth() > 0) { save = all.removeFromRight(static_cast<float>(saveWidth()) - 8.f); all.removeFromRight(8.f); }
 const auto r = all;
 const int count = getCount ? getCount() : 0, current = getCurrent ? getCurrent() : 0;
 const juce::String name = userName.isNotEmpty() ? userName : getName ? getName(current) : juce::String();
 const juce::String counter = userName.isNotEmpty() ? juce::String("YOUR PRESET") : juce::String(current + 1).paddedLeft('0', 2) + " / " + juce::String(count);
 if (vintage) { // a black program window with amber lettering
  juce::Path p;
  p.addRoundedRectangle(r, 5.f);
  juce::DropShadow(juce::Colour(0xcc000000), 10, {2, 4}).drawForPath(g, p);
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2a231d), 0.f, r.getY(), juce::Colour(0xff0f0c0a), 0.f, r.getBottom(), false));
  g.fillPath(p);
  g.setColour(juce::Colour(0xff8f877a));
  g.drawRoundedRectangle(r.reduced(.5f), 5.f, 1.5f);
  const auto glass = r.reduced(44.f, 7.f);
  g.setColour(juce::Colour(0xff120a04));
  g.fillRoundedRectangle(glass, 3.f);
  g.setColour(colour::amber.withAlpha(.8f));
  g.setFont(uiFont(26.f));
  g.drawText(juce::String::fromUTF8("\xe2\x80\xb9"), r.withWidth(44.f).translated(2.f, -2.f), juce::Justification::centred);
  g.drawText(juce::String::fromUTF8("\xe2\x80\xba"), r.withTrimmedLeft(r.getWidth() - 44.f).translated(-2.f, -2.f), juce::Justification::centred);
  g.setFont(monoFont(10.5f));
  g.setColour(colour::amber.withAlpha(.6f));
  g.drawText(userName.isNotEmpty() ? counter : "PROGRAM " + counter, glass.withHeight(glass.getHeight() * .42f).translated(0.f, 2.f), juce::Justification::centred);
  g.setColour(colour::amber.withAlpha(.25f));
  const auto nameArea = glass.withTrimmedTop(glass.getHeight() * .36f).toNearestInt();
  g.setFont(titleFont(16.f));
  g.drawFittedText(name.toUpperCase(), nameArea.translated(0, 1).expanded(1, 0), juce::Justification::centred, 1, .8f);
  g.setColour(colour::amber.brighter(.3f));
  g.drawFittedText(name.toUpperCase(), nameArea, juce::Justification::centred, 1, .8f);
  if (!save.isEmpty()) {
   g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2a231d), 0.f, save.getY(), juce::Colour(0xff0f0c0a), 0.f, save.getBottom(), false));
   g.fillRoundedRectangle(save, 5.f);
   g.setColour(juce::Colour(0xff8f877a));
   g.drawRoundedRectangle(save.reduced(.5f), 5.f, 1.5f);
   g.setColour(colour::amber.brighter(.2f));
   g.setFont(titleFont(14.f));
   g.drawText("SAVE", save, juce::Justification::centred);
  }
  return;
 }
 juce::Path p;
 p.addRoundedRectangle(r, r.getHeight() * .5f);
 juce::DropShadow(colour::shadow.withAlpha(.4f), 12, {3, 4}).drawForPath(g, p);
 juce::DropShadow(juce::Colours::white, 8, {-3, -3}).drawForPath(g, p);
 g.setGradientFill(juce::ColourGradient(juce::Colours::white, 0.f, r.getY(), colour::card, 0.f, r.getBottom(), false));
 g.fillPath(p);
 g.setColour(colour::ink.withAlpha(.7f));
 g.setFont(uiFont(30.f));
 g.drawText(juce::String::fromUTF8("\xe2\x80\xb9"), r.withWidth(44.f).translated(6.f, -2.f), juce::Justification::centred);
 g.drawText(juce::String::fromUTF8("\xe2\x80\xba"), r.withTrimmedLeft(r.getWidth() - 44.f).translated(-6.f, -2.f), juce::Justification::centred);
 const auto mid = r.reduced(40.f, 0.f);
 g.setFont(monoFont(10.5f));
 g.setColour(colour::muted);
 g.drawText(counter, mid.withHeight(mid.getHeight() * .42f).translated(0.f, 3.f), juce::Justification::centred);
 g.setColour(colour::ink);
 g.setFont(uiFont(16.f, true));
 g.drawFittedText(name, mid.withTrimmedTop(mid.getHeight() * .36f).toNearestInt(), juce::Justification::centred, 1, .8f);
 if (!save.isEmpty()) {
  juce::Path sp;
  sp.addRoundedRectangle(save, save.getHeight() * .5f);
  juce::DropShadow(colour::shadow.withAlpha(.4f), 10, {2, 3}).drawForPath(g, sp);
  g.setGradientFill(juce::ColourGradient(juce::Colours::white, 0.f, save.getY(), colour::card, 0.f, save.getBottom(), false));
  g.fillPath(sp);
  g.setColour(colour::ink);
  g.setFont(uiFont(11.f, true));
  g.drawText("SAVE", save, juce::Justification::centred);
 }
}

void PresetBar::saveDialog() {
 if (userApvts == nullptr) return;
 auto* window = new juce::AlertWindow("Save preset", "A name for this sound:", juce::MessageBoxIconType::NoIcon, this);
 window->addTextEditor("name", userName.isNotEmpty() ? userName : juce::String("My ") + (getName ? getName(getCurrent ? getCurrent() : 0) : juce::String("sound")));
 window->addButton("SAVE", 1, juce::KeyPress(juce::KeyPress::returnKey));
 window->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));
 juce::Component::SafePointer<PresetBar> safe(this);
 window->enterModalState(true, juce::ModalCallbackFunction::create([safe, window](int result) {
  if (safe == nullptr || result != 1) return;
  const auto name = window->getTextEditorContents("name").trim();
  if (abismo_ui::saveUserPreset(*safe->userApvts, safe->userPlugin, name)) { safe->userName = juce::File::createLegalFileName(name); safe->repaint(); }
 }), true);
}

void PresetBar::mouseUp(const juce::MouseEvent& e) {
 if (!getCount || !select) return;
 const int count = getCount(), current = getCurrent();
 if (count <= 0) return;
 const int width = getWidth() - saveWidth();
 if (saveWidth() > 0 && e.x >= width) { saveDialog(); return; }
 if (e.x < 48) { userName = {}; select((current + count - 1) % count); repaint(); return; }
 if (e.x > width - 48 && e.x < width) { userName = {}; select((current + 1) % count); repaint(); return; }
 juce::PopupMenu menu;
 if (userApvts != nullptr) menu.addSectionHeader("FACTORY");
 for (int i = 0; i < count; ++i) menu.addItem(i + 1, getName(i), true, i == current && userName.isEmpty());
 juce::Array<juce::File> files;
 if (userApvts != nullptr) {
  files = abismo_ui::userPresets(userPlugin);
  menu.addSectionHeader("YOURS");
  if (files.isEmpty()) menu.addItem(-1, "None yet: press SAVE to keep the current sound", false);
  for (int i = 0; i < files.size(); ++i) menu.addItem(10000 + i, files[i].getFileNameWithoutExtension(), true, files[i].getFileNameWithoutExtension() == userName);
  menu.addSeparator();
  menu.addItem(20000, "Save the current sound...");
  menu.addItem(20001, "Open the presets folder");
 }
 juce::Component::SafePointer<PresetBar> safe(this);
 menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMaximumNumColumns(2), [safe, files](int chosen) {
  if (safe == nullptr || chosen <= 0) return;
  if (chosen == 20000) { safe->saveDialog(); return; }
  if (chosen == 20001) { const auto f = abismo_ui::userPresetFolder(safe->userPlugin); f.createDirectory(); f.revealToUser(); return; }
  if (chosen >= 10000) {
   const auto& file = files[chosen - 10000];
   if (abismo_ui::loadUserPreset(*safe->userApvts, file)) safe->userName = file.getFileNameWithoutExtension();
   safe->repaint();
   return;
  }
  safe->userName = {};
  safe->select(chosen - 1);
  safe->repaint();
 });
}

void Body::paint(juce::Graphics& g) {
 const auto r = getLocalBounds().toFloat();
 if (vintage) {
  // Walnut veneer, drawn once.
  if (woodCache.getWidth() != getWidth() || woodCache.getHeight() != getHeight()) {
   woodCache = juce::Image(juce::Image::RGB, std::max(1, getWidth()), std::max(1, getHeight()), false);
   juce::Graphics wg(woodCache);
   wg.setGradientFill(juce::ColourGradient(colour::bodyTop, 0.f, 0.f, colour::bodyBottom, r.getRight(), r.getBottom(), false));
   wg.fillAll();
   juce::Random rnd(7);
   for (int i = 0; i < 260; ++i) { // grain lines that wave slowly across
    const float y0 = rnd.nextFloat() * r.getHeight(), amp = 2.f + rnd.nextFloat() * 10.f, freq = .002f + rnd.nextFloat() * .006f, ph = rnd.nextFloat() * 6.f;
    juce::Path grain;
    grain.startNewSubPath(0.f, y0);
    for (float x = 0.f; x <= r.getWidth(); x += 12.f) grain.lineTo(x, y0 + amp * std::sin(x * freq + ph) + 3.f * std::sin(x * freq * 5.3f + ph * 2.f));
    wg.setColour((rnd.nextBool() ? juce::Colour(0xff1e0f06) : juce::Colour(0xff7a4a2c)).withAlpha(.08f + .14f * rnd.nextFloat()));
    wg.strokePath(grain, juce::PathStrokeType(.6f + rnd.nextFloat() * 1.8f));
   }
   wg.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(), juce::Colour(0x66000000), 0.f, 0.f, true));
   wg.fillAll();
  }
  g.drawImageAt(woodCache, 0, 0);
  // Brass name plate.
  const juce::Rectangle<float> plate(24.f, 14.f, 370.f, 66.f);
  g.setColour(juce::Colour(0x88000000));
  g.fillRoundedRectangle(plate.translated(2.f, 3.f), 4.f);
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe2c26e), plate.getX(), plate.getY(), juce::Colour(0xff9a7330), plate.getRight(), plate.getBottom(), false));
  g.fillRoundedRectangle(plate, 4.f);
  g.setColour(juce::Colour(0x55ffffff));
  g.drawRoundedRectangle(plate.reduced(1.f), 4.f, 1.f);
  screw(g, {plate.getX() + 10.f, plate.getCentreY()}, 4.f, .4f);
  screw(g, {plate.getRight() - 10.f, plate.getCentreY()}, 4.f, 1.2f);
  g.setColour(juce::Colour(0xff3b2a12));
  g.setFont(uiFont(9.f, true).withExtraKerningFactor(.35f));
  g.drawText("INVERSO", plate.withTrimmedLeft(24.f).withHeight(18.f).translated(0.f, 5.f), juce::Justification::centredLeft);
  g.setFont(titleFont(26.f));
  g.drawText(title, plate.withTrimmedLeft(24.f).withTrimmedTop(18.f).withHeight(34.f), juce::Justification::centredLeft);
  g.setColour(juce::Colour(0xffe8d6b0).withAlpha(.75f));
  g.setFont(uiFont(10.f).withExtraKerningFactor(.08f));
  g.drawFittedText(subtitle, juce::Rectangle<int>(getWidth() - 430, 20, 400, 60), juce::Justification::centredRight, 3, .9f);
  return;
 }
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
