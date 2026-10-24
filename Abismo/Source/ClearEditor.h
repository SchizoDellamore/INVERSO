#pragma once
#include <JuceHeader.h>
#include "SoftUI.h"
#include "ClearProcessor.h"

// The big screen: the sound's spectrum, and over it what CLEAR takes away.
class ClearAnalyzer final : public juce::Component {
public:
 explicit ClearAnalyzer(InversoClearProcessor& p) : processor(p) {}
 void paint(juce::Graphics&) override;
 void resized() override { cardCache = {}; }
 void update();
private:
 InversoClearProcessor& processor;
 std::array<float, inverso_abismo::clear::Engine::displayBins> level{}, cut{}, peakCut{};
 juce::Image cardCache;
};

class InversoClearEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
 explicit InversoClearEditor(InversoClearProcessor&);
 ~InversoClearEditor() override;
 void resized() override;
 void refresh() { timerCallback(); } // what the timer does, on demand (tests, snapshots)
private:
 void timerCallback() override;
 InversoClearProcessor& processor;
 soft::Look look;
 soft::Body body;
 soft::PresetBar presets;
 ClearAnalyzer analyzer;
 std::vector<std::unique_ptr<soft::Module>> modules;
 std::unique_ptr<soft::Master> master;
 juce::TooltipWindow tooltips{this, 600};
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoClearEditor)
};
