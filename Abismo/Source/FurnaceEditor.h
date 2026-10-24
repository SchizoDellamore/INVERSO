#pragma once
#include <JuceHeader.h>
#include "SoftUI.h"
#include "FurnaceProcessor.h"

class InversoFurnaceEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
 explicit InversoFurnaceEditor(InversoFurnaceProcessor&);
 ~InversoFurnaceEditor() override;
 void resized() override;
private:
 void timerCallback() override;
 float value(int param) const;
 void chooseIR();
 // Switches the shared UI to its vintage look before any component is built.
 struct Vintage { Vintage() { soft::useVintage(); } } vintage;
 InversoFurnaceProcessor& processor;
 soft::Look look;
 soft::Body body;
 soft::PresetBar presets;
 std::vector<std::unique_ptr<soft::Module>> modules;
 std::unique_ptr<soft::Master> master;
 juce::TextButton loadIR{"LOAD IR..."};
 std::unique_ptr<juce::FileChooser> chooser;
 juce::TooltipWindow tooltips{this, 600};
 float animation = 0.f;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoFurnaceEditor)
};
