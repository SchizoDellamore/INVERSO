#pragma once
// Soft hardware look shared by ABISMO, ABISMO SAMPLER and ABISMO DRUMS: a light
// body, raised cards (one accent colour per module), dark screens that draw
// what the sound is doing, white knobs with a coloured value arc, pages so a
// module never shows more than a handful of controls at once.
#include <JuceHeader.h>
#include "AbismoUI.h"
#include <functional>
#include <memory>
#include <vector>

namespace soft {

// The palette. Each plugin is its own binary, so an editor may switch the whole
// look to vintage (useVintage) before it builds any component.
namespace colour {
inline juce::Colour bodyTop{0xffeef1f5}, bodyBottom{0xffe2e6ec};
inline juce::Colour card{0xfff4f6f9}, cardEdge{0xffffffff};
inline juce::Colour shadow{0xff9aa6b8};
inline juce::Colour ink{0xff343b48}, muted{0xff8b94a4}, faint{0xffc9d0da};
inline juce::Colour track{0xffdde2e9};
inline juce::Colour screenTop{0xff1d2129}, screenBottom{0xff111318};
inline juce::Colour screenInk{0xffe9edf3}, screenMuted{0xff7b8496};
// Module accents.
inline juce::Colour amber{0xfff2a541}, cyan{0xff3ccfe0}, violet{0xff9a86ff}, red{0xffef5350};
inline juce::Colour orange{0xffff8a3d}, green{0xff52d39a}, pink{0xfff06bb0}, blue{0xff5aa9f5};
inline juce::Colour blood{0xffd23a3f};
}

// Vintage look: walnut body, cream enamel panels with screws, bakelite knobs
// over a printed scale, phosphor screens, a VU meter. Call before building the UI.
inline bool vintage = false;
void useVintage();

// Embedded typefaces (OFL): Poppins / IBM Plex Mono, or Oswald / Courier Prime when vintage.
juce::Font uiFont(float height, bool bold = false);
juce::Font monoFont(float height);
juce::Font titleFont(float height); // module and instrument titles (Bebas Neue when vintage)

class Look final : public juce::LookAndFeel_V4 {
public:
 Look();
 void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
 void drawComboBox(juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
 void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
 juce::Font getComboBoxFont(juce::ComboBox&) override;
 juce::Font getPopupMenuFont() override;
 void drawPopupMenuBackground(juce::Graphics&, int w, int h) override;
 void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
 void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
 juce::Font getTextButtonFont(juce::TextButton&, int h) override;
 juce::Typeface::Ptr getTypefaceForFont(const juce::Font&) override;
};

// Paints the raised card used behind every module.
void paintCard(juce::Graphics&, juce::Rectangle<float>, float radius = 14.f);
// Paints a dark screen with its glass edge.
void paintScreen(juce::Graphics&, juce::Rectangle<float>, float radius = 10.f);
// A glowing line on a screen.
void glowStroke(juce::Graphics&, const juce::Path&, juce::Colour, float width = 1.8f);

class Knob final : public juce::Slider {
public:
 explicit Knob(juce::Colour accent);
 void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
};

// A dark display inside a module. `draw` gets the inner area and the current page.
class Screen final : public juce::Component {
public:
 std::function<void(juce::Graphics&, juce::Rectangle<float>, int page)> draw;
 int page = 0;
 void paint(juce::Graphics&) override;
};

// A choice parameter shown on a screen as  ‹ VALUE ›  with clickable arrows.
class ScreenChoice final : public juce::Component {
public:
 ScreenChoice(juce::AudioProcessorValueTreeState&, const inverso_abismo::ParamSpec&, juce::String caption, juce::Colour accent);
 void paint(juce::Graphics&) override;
 void mouseUp(const juce::MouseEvent&) override;
private:
 void step(int delta);
 juce::RangedAudioParameter* param;
 juce::ParameterAttachment attachment;
 juce::StringArray items;
 juce::String caption;
 juce::Colour accent;
 int index = 0;
};

// One module: a card with a title, page tabs, an optional screen and controls.
class Module final : public juce::Component {
public:
 struct Page { juce::String name; std::vector<int> params; std::vector<int> big = {}; };
 Module(juce::AudioProcessorValueTreeState&, const inverso_abismo::ParamSpec* specs, const char* const* shortNames,
        juce::String title, juce::Colour accent, std::vector<Page> pages, int screenHeight = 0,
        std::vector<int> screenChoices = {});
 Screen screen;
 int page() const { return current; }
 void setPage(int);
 // Dims controls that currently have no effect (they stay adjustable).
 void dimInactive(const std::function<bool(int)>& isActive);
 // Extra components placed in the header row, right-aligned (buttons, readouts).
 void addHeaderComponent(juce::Component&, int width);
 juce::Colour accent;
 void paint(juce::Graphics&) override;
 void resized() override;
 void mouseUp(const juce::MouseEvent&) override;
private:
 using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
 using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
 using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
 struct Cell {
  int param = 0, page = 0;
  bool big = false;
  std::unique_ptr<juce::Label> name, value;
  std::unique_ptr<Knob> knob;
  std::unique_ptr<juce::ComboBox> combo;
  std::unique_ptr<juce::ToggleButton> toggle;
  std::unique_ptr<SA> sa;
  std::unique_ptr<CA> ca;
  std::unique_ptr<BA> ba;
  std::vector<juce::Component*> parts() const;
 };
 juce::Rectangle<int> tabArea(int index) const;
 const inverso_abismo::ParamSpec* specs;
 juce::String title;
 std::vector<Page> pages;
 std::vector<Cell> cells;
 std::vector<std::unique_ptr<ScreenChoice>> choices;
 std::vector<std::pair<juce::Component*, int>> headerParts;
 int screenHeight = 0, current = 0;
 juce::Image cardCache;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Module)
};

// The master strip: a big output knob, an optional second knob, a level meter
// and a small readout line (voices, peak).
class Master final : public juce::Component {
public:
 Master(juce::AudioProcessorValueTreeState&, const inverso_abismo::ParamSpec& output, const inverso_abismo::ParamSpec* second,
        juce::String secondName, juce::Colour accent);
 void setReadout(float peak, juce::String line);
 void paint(juce::Graphics&) override;
 void resized() override;
private:
 Knob output, second;
 juce::Label outValue, secondValue;
 std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outAttach, secondAttach;
 bool hasSecond = false;
 juce::String secondName, readout;
 class Meter* meterPtr = nullptr;
 std::unique_ptr<juce::Component> meter;
 float peakDb = -90.f;
 juce::Image cardCache;
};

// Vertical stereo peak meter.
// Vintage: an analogue VU meter with a needle (0 VU = -18 dBFS).
class Meter final : public juce::Component {
public:
 void setLevel(float peak);
 void paint(juce::Graphics&) override;
private:
 float shown = 0.f, needle = -20.f;
};

// Preset browser:  ‹  NAME  ›  in a raised pill; clicking the name opens the list.
class PresetBar final : public juce::Component {
public:
 std::function<int()> getCount, getCurrent;
 std::function<juce::String(int)> getName;
 std::function<void(int)> select;
 // Repaints only when the current program or the count changed.
 void refresh() {
  const int cur = getCurrent ? getCurrent() : 0, n = getCount ? getCount() : 0;
  if (cur != shown || n != shownCount) { shown = cur; shownCount = n; repaint(); }
 }
 void paint(juce::Graphics&) override;
 void mouseUp(const juce::MouseEvent&) override;
private:
 int shown = -1, shownCount = -1;
};

// The editor body: a light background, a header with the instrument name,
// a preset bar and a meter; everything else is laid out by the editor on a
// fixed design size and scaled when the window is resized.
class Body final : public juce::Component {
public:
 juce::String title, subtitle;
 void paint(juce::Graphics&) override;
private:
 juce::Image woodCache;
};

} // namespace soft
