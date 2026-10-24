#pragma once
// Look, knobs and the parameter panel shared by ABISMO and ABISMO SAMPLER, plus
// the APVTS layout builder for a portable ParamSpec table.
#include <JuceHeader.h>
#include "DSP/AbismoParams.h"
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace abismo_ui {

inline const juce::Colour bg{10,10,13};
inline const juce::Colour panel{21,21,26};
inline const juce::Colour text{229,221,206};
inline const juce::Colour muted{140,136,130};
inline const juce::Colour blood{176,38,42};
inline const juce::Colour bone{155,147,131};

// MIDI learn for every parameter: arm a parameter (right-click a knob), turn a
// hardware knob, and that CC moves it from then on. The audio thread only reads
// and writes atomics; the parameter itself is set on the message thread.
class MidiLearn final : private juce::Timer {
public:
 explicit MidiLearn(juce::AudioProcessor&);
 ~MidiLearn() override;
 // Audio thread: every MIDI message goes through here (controllers are used, the rest ignored).
 void handle(const juce::MidiMessage&);
 // Message thread.
 void arm(int parameterIndex);
 void disarm() { armed.store(-1); }
 int armedParameter() const { return armed.load(); }
 int ccFor(int parameterIndex) const;
 void forget(int parameterIndex);
 int indexOf(const juce::String& parameterID) const;
 void save(juce::ValueTree& state) const;
 void load(const juce::ValueTree& state);
 // The learn object of the processor that owns this parameter tree (for the knobs).
 static MidiLearn* find(const juce::AudioProcessor*);
 // Applies the CC values the audio thread received (the timer calls it 60 times a second).
 void flush();
private:
 void timerCallback() override { flush(); }
 juce::AudioProcessor& processor;
 std::array<std::atomic<int>,128> map, pending;
 std::atomic<int> armed{-1};
 int learn(const juce::MidiMessage&); // the CC (or -1), after completing an armed learn with it
};

// A MIDI pitch wheel (0..16383, centre 8192) as -1..+1 (both ends reached), the way every instrument reads it.
inline float pitchBendOf(int wheel) { const float d=static_cast<float>(wheel)-8192.f; return d/(d>=0.f?8191.f:8192.f); }

// Your own presets: XML files in Documents/INVERSO/<plugin>/Presets.
juce::File userPresetFolder(const juce::String& plugin);
juce::Array<juce::File> userPresets(const juce::String& plugin);
bool saveUserPreset(juce::AudioProcessorValueTreeState&,const juce::String& plugin,const juce::String& name);
bool loadUserPreset(juce::AudioProcessorValueTreeState&,const juce::File&);

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
