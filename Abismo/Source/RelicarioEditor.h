#pragma once
#include <JuceHeader.h>
#include "SoftUI.h"
#include "RelicarioProcessor.h"

class InversoRelicarioEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
 explicit InversoRelicarioEditor(InversoRelicarioProcessor&);
 ~InversoRelicarioEditor() override;
 void resized() override;
private:
 void timerCallback() override;
 float value(int param) const;
 // Switches the shared UI to its vintage look before any component is built.
 struct Vintage { Vintage(){ soft::useVintage(); } } vintage;
 InversoRelicarioProcessor& processor;
 soft::Look look;
 soft::Body body;
 soft::PresetBar presets;
 std::vector<std::unique_ptr<soft::Module>> modules;
 std::unique_ptr<soft::Master> master;
 std::array<juce::TextButton,11> onButtons;
 std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>,11> onAttach;
 juce::TooltipWindow tooltips{this,600};
 float animation=0.f;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoRelicarioEditor)
};
