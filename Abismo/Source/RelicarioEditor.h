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
 InversoRelicarioProcessor& processor;
 soft::Look look;
 soft::Body body;
 soft::PresetBar presets;
 std::vector<std::unique_ptr<soft::Module>> modules;
 std::unique_ptr<soft::Master> master;
 juce::TooltipWindow tooltips{this,600};
 float animation=0.f;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoRelicarioEditor)
};
