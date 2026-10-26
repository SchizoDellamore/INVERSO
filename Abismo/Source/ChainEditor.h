#pragma once
#include <JuceHeader.h>
#include "SoftUI.h"
#include "ChainProcessor.h"

class InversoChainEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
 explicit InversoChainEditor(InversoChainProcessor&);
 ~InversoChainEditor() override;
 void resized() override;
 void refresh() { timerCallback(); } // what the timer does, on demand (tests, snapshots)
private:
 class Loudness; // the LUFS readout of the master
 void timerCallback() override;
 void setupScreens();
 float v(int global) const; // a parameter of the table (the editor's snapshot, refreshed by the timer)
 InversoChainProcessor& processor;
 inverso_abismo::chain::Values values{};
 soft::Look look;
 soft::Body body;
 soft::PresetBar presets;
 enum class Part { Macro, Gate, Trans, Eq, Tame, Comp, Glue, Sat, Width, Limiter };
 struct Entry { std::unique_ptr<soft::Module> module; Part part = Part::Eq; bool hasChoice = false; int weight = 1; int row = 0; };
 std::vector<Entry> modules;
 std::unique_ptr<soft::Master> master;
 Loudness* loudness = nullptr;
 std::array<float, 120> gateHistory{}, limiterHistory{};
 inverso_abismo::chain::Saturator sat; // the saturation screen's own
 int designW = 1280, designH = 780;
 juce::TooltipWindow tooltips{this, 600};
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoChainEditor)
};
