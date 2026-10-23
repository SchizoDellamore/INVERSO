#pragma once
#include <JuceHeader.h>
#include "AbismoUI.h"
#include "PluginProcessor.h"

class InversoAbismoEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
 explicit InversoAbismoEditor(InversoAbismoProcessor&);
 ~InversoAbismoEditor() override;
 void paint(juce::Graphics&) override;
 void resized() override;
private:
 void timerCallback() override;
 void refreshPresetBox();
 InversoAbismoProcessor& processor;
 abismo_ui::Look look;
 std::unique_ptr<abismo_ui::ParameterPanel> panel;
 juce::ComboBox presetBox;
 juce::TextButton previous{"<"},next{">"};
 float shownPeak=0.f;
 int shownVoices=0;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoAbismoEditor)
};
