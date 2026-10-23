#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <memory>
#include <vector>

class AbismoLook final : public juce::LookAndFeel_V4 {
public:
 AbismoLook();
 void drawRotarySlider(juce::Graphics&,int x,int y,int w,int h,float pos,float start,float end,juce::Slider&) override;
};

class AbismoKnob final : public juce::Slider {
public:
 AbismoKnob();
 void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
};

class InversoAbismoEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
 explicit InversoAbismoEditor(InversoAbismoProcessor&);
 ~InversoAbismoEditor() override;
 void paint(juce::Graphics&) override;
 void resized() override;
private:
 using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
 using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
 using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
 struct Control {
  int param=0;
  std::unique_ptr<juce::Label> label;
  std::unique_ptr<AbismoKnob> knob;
  std::unique_ptr<juce::ComboBox> combo;
  std::unique_ptr<juce::ToggleButton> toggle;
  std::unique_ptr<SA> sliderAttachment;
  std::unique_ptr<CA> comboAttachment;
  std::unique_ptr<BA> buttonAttachment;
 };
 struct Section { juce::String title; std::vector<int> params; juce::Rectangle<int> bounds; };
 void timerCallback() override;
 void refreshPresetBox();
 InversoAbismoProcessor& processor;
 AbismoLook look;
 std::vector<Section> sections;
 std::vector<Control> controls;
 juce::ComboBox presetBox;
 juce::TextButton previous{"<"},next{">"};
 float shownPeak=0.f;
 int shownVoices=0;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoAbismoEditor)
};
