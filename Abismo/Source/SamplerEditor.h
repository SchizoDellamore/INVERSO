#pragma once
#include <JuceHeader.h>
#include "AbismoUI.h"
#include "SamplerProcessor.h"

// Waveform with start / loop / grain-position markers and the live playhead.
// Clicking sets START (classic) or POSITION (granular) at that point.
class SampleView final : public juce::Component {
public:
 explicit SampleView(InversoAbismoSamplerProcessor&);
 void refresh();
 void paint(juce::Graphics&) override;
 void mouseDown(const juce::MouseEvent&) override;
 void mouseDrag(const juce::MouseEvent&) override;
 void resized() override {refresh();}
private:
 void setFromX(float x);
 float param(const char* id) const;
 InversoAbismoSamplerProcessor& processor;
 std::vector<std::pair<float,float>> peaks;
};

class InversoAbismoSamplerEditor final : public juce::AudioProcessorEditor,public juce::FileDragAndDropTarget,
                                         private juce::Timer {
public:
 explicit InversoAbismoSamplerEditor(InversoAbismoSamplerProcessor&);
 ~InversoAbismoSamplerEditor() override;
 void paint(juce::Graphics&) override;
 void resized() override;
 bool isInterestedInFileDrag(const juce::StringArray&) override;
 void filesDropped(const juce::StringArray&,int,int) override;
 void fileDragEnter(const juce::StringArray&,int,int) override {dragging=true;repaint();}
 void fileDragExit(const juce::StringArray&) override {dragging=false;repaint();}
 // Pull sample, status and meters from the processor (the timer does this 25x/s).
 void refreshFromProcessor();
private:
 void timerCallback() override {refreshFromProcessor();}
 void load(const juce::File&);
 InversoAbismoSamplerProcessor& processor;
 abismo_ui::Look look;
 std::unique_ptr<abismo_ui::ParameterPanel> panel;
 SampleView view;
 juce::TextButton loadButton{"CARREGAR SAMPLE"},builtInButton{"VOZ INTERNA"};
 juce::ComboBox presetBox;
 juce::TextButton previous{"<"},next{">"};
 std::unique_ptr<juce::FileChooser> chooser;
 juce::String status;
 int shownVersion=-1;
 float shownPeak=0.f;
 int shownVoices=0;
 bool dragging=false;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoAbismoSamplerEditor)
};
