#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
#include <memory>

class WheelSlider final : public juce::Slider {
public:
 void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
};
class InversoBassDriveEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
 explicit InversoBassDriveEditor(InversoBassDriveProcessor&);
 ~InversoBassDriveEditor()override=default;
 void paint(juce::Graphics&)override;
 void resized()override;
private:
 using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
 using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
 using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
 void timerCallback()override{repaint();}
 InversoBassDriveProcessor& processor;
 std::array<WheelSlider,8> sliders;
 std::array<juce::Label,8> labels;
 std::array<std::unique_ptr<SA>,8> attachments;
 juce::ComboBox modes;
 std::unique_ptr<CA> modeAttachment;
 juce::Label modeLabel;
 juce::Label gruntLabel;
 juce::ComboBox grunt;
 std::unique_ptr<CA> gruntAttachment;
 juce::ToggleButton midBoost{"MID BOOST"};
 std::unique_ptr<BA> midBoostAttachment;
 juce::ToggleButton lowProtect{"PRESERVE LOW"};
 juce::ToggleButton bypass{"BYPASS"};
 std::unique_ptr<BA> bypassAttachment;
 std::unique_ptr<BA> lowProtectAttachment;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoBassDriveEditor)
};
