#pragma once
#include <JuceHeader.h>
#include "../../Abismo/Source/AbismoUI.h"
#include "PluginProcessor.h"

// Drawn berimbau: the bow (verga), the wire, the gourd that opens with GOURD /
// mod wheel, and the stone pressing as aftertouch arrives. Purely a display.
class BerimbauView final : public juce::Component {
public:
 explicit BerimbauView(InversoBerimbassProcessor& p):processor(p){}
 void paint(juce::Graphics&) override;
private:
 InversoBerimbassProcessor& processor;
};

class InversoBerimbassEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
 explicit InversoBerimbassEditor(InversoBerimbassProcessor&);
 ~InversoBerimbassEditor() override;
 void paint(juce::Graphics&) override;
 void resized() override;
private:
 void timerCallback() override;
 InversoBerimbassProcessor& processor;
 abismo_ui::Look look;
 std::unique_ptr<abismo_ui::ParameterPanel> panel;
 BerimbauView view;
 juce::ComboBox presetBox;
 juce::TextButton previous{"<"},next{">"};
 float shownPeak=0.f;
 int shownVoices=0;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoBerimbassEditor)
};
