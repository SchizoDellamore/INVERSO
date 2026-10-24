#pragma once
#include <JuceHeader.h>
#include "SoftUI.h"
#include "NeonProcessor.h"

class InversoNeonEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
 explicit InversoNeonEditor(InversoNeonProcessor&);
 ~InversoNeonEditor() override;
 void resized() override;
 void refresh() { timerCallback(); } // what the timer does, on demand (tests, snapshots)
private:
 void timerCallback() override;
 float value(int param) const;
 InversoNeonProcessor& processor;
 soft::Look look;
 soft::Body body;
 soft::PresetBar presets;
 std::vector<std::unique_ptr<soft::Module>> modules;
 juce::TextButton arpOn;
 std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> arpAttach;
 std::unique_ptr<soft::Master> master;
 juce::TooltipWindow tooltips{this, 600};
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoNeonEditor)
};
