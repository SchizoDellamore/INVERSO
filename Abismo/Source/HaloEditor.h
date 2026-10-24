#pragma once
#include <JuceHeader.h>
#include "SoftUI.h"
#include "HaloProcessor.h"

class InversoHaloEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
 explicit InversoHaloEditor(InversoHaloProcessor&);
 ~InversoHaloEditor() override;
 void resized() override;
 void refresh() { timerCallback(); } // what the timer does, on demand (tests, snapshots)
private:
 void timerCallback() override;
 float value(int param) const;
 InversoHaloProcessor& processor;
 soft::Look look;
 soft::Body body;
 soft::PresetBar presets;
 std::vector<std::unique_ptr<soft::Module>> modules;
 std::unique_ptr<soft::Master> master;
 std::array<float, 120> gateHistory{};
 juce::TooltipWindow tooltips{this, 600};
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoHaloEditor)
};
