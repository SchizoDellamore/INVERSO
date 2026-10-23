#pragma once
#include <JuceHeader.h>
#include "SoftUI.h"
#include "PluginProcessor.h"

class InversoAbismoEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
 explicit InversoAbismoEditor(InversoAbismoProcessor&);
 ~InversoAbismoEditor() override;
 void resized() override;
private:
 void timerCallback() override;
 float value(int param) const;
 InversoAbismoProcessor& processor;
 soft::Look look;
 soft::Body body;
 soft::PresetBar presets;
 std::vector<std::unique_ptr<soft::Module>> modules;
 std::unique_ptr<soft::Master> master;
 juce::TooltipWindow tooltips{this,600};
 float animation=0.f;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoAbismoEditor)
};
