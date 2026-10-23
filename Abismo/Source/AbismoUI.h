#pragma once
// Look, knobs and the parameter panel shared by ABISMO and ABISMO SAMPLER, plus
// the APVTS layout builder for a portable ParamSpec table.
#include <JuceHeader.h>
#include "DSP/AbismoParams.h"
#include <memory>
#include <vector>

namespace abismo_ui {

inline const juce::Colour bg{10,10,13};
inline const juce::Colour panel{21,21,26};
inline const juce::Colour text{229,221,206};
inline const juce::Colour muted{140,136,130};
inline const juce::Colour blood{176,38,42};
inline const juce::Colour bone{155,147,131};

juce::String formatValue(const inverso_abismo::ParamSpec&,double);
juce::AudioProcessorValueTreeState::ParameterLayout makeLayout(const inverso_abismo::ParamSpec* specs,int count);

class Look final : public juce::LookAndFeel_V4 {
public:
 Look();
 void drawRotarySlider(juce::Graphics&,int x,int y,int w,int h,float pos,float start,float end,juce::Slider&) override;
};

class Knob final : public juce::Slider {
public:
 Knob();
 void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
};

// Titled sections of knobs / menus / toggles attached to an APVTS, laid out in
// rows that wrap and stretch to the component's width.
class ParameterPanel final : public juce::Component {
public:
 struct Section { juce::String title; std::vector<int> params; };
 ParameterPanel(juce::AudioProcessorValueTreeState&,const inverso_abismo::ParamSpec* specs,
                std::vector<Section> sections,const char* const* shortNames);
 int heightForWidth(int width) const;
 // Dims the controls that currently have no effect (they stay adjustable).
 void dimInactive(const std::function<bool(int)>& isActive);
 void paint(juce::Graphics&) override;
 void resized() override;
private:
 using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
 using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
 using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
 struct Control {
  int param=0;
  std::unique_ptr<juce::Label> label;
  std::unique_ptr<Knob> knob;
  std::unique_ptr<juce::ComboBox> combo;
  std::unique_ptr<juce::ToggleButton> toggle;
  std::unique_ptr<SA> sliderAttachment;
  std::unique_ptr<CA> comboAttachment;
  std::unique_ptr<BA> buttonAttachment;
 };
 int naturalWidth(int param) const;
 int naturalWidth(const Section&) const;
 std::vector<juce::Rectangle<int>> layoutSections(int width) const;
 const inverso_abismo::ParamSpec* specs;
 std::vector<Section> sections;
 std::vector<juce::Rectangle<int>> bounds;
 std::vector<Control> controls;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterPanel)
};

} // namespace abismo_ui
