#pragma once
#include "PluginProcessor.h"
#include <memory>

class InversoWheelSlider final:public juce::Slider {
public:
 void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel)override;
};
class InversoMeterPanel final:public juce::Component,private juce::Timer {
public:
 explicit InversoMeterPanel(InversoProcessor& p):processor(p){startTimerHz(20);}
 void paint(juce::Graphics&)override;
private:
 void timerCallback()override{repaint();}
 InversoProcessor& processor;
};
class InversoProcessorEditor final:public juce::AudioProcessorEditor {
public:
 explicit InversoProcessorEditor(InversoProcessor& p);
 void paint(juce::Graphics&)override;
 void resized()override;
private:
 InversoProcessor& processor;
 std::array<InversoWheelSlider,3> sliders;
 std::array<juce::Label,3> labels;
 juce::ToggleButton bypass{"BYPASS"},delta{"LISTEN REMOVED"};
 InversoMeterPanel meter;
 using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
 using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
 std::array<std::unique_ptr<SA>,3> attachments;
 std::unique_ptr<BA> bypassAttach,deltaAttach;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoProcessorEditor)
};
