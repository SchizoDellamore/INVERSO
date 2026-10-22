#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
#include <memory>

class InversoDrumEditor final : public juce::AudioProcessorEditor,
                                private juce::Timer, public juce::FileDragAndDropTarget {
public:
 explicit InversoDrumEditor(InversoDrumProcessor&);
 ~InversoDrumEditor() override=default;
 void paint(juce::Graphics&) override;
 void resized() override;
 bool isInterestedInFileDrag(const juce::StringArray&) override;
 void filesDropped(const juce::StringArray&,int,int) override;
private:
 class StepGrid final : public juce::Component {
 public:
  explicit StepGrid(InversoDrumEditor& e):editor(e){}
  void paint(juce::Graphics&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;
 private:
  InversoDrumEditor& editor;
  int previousCell=-1;
 };
 void timerCallback() override;
 void selectPad(int);
 void updateUi();
 void loadSelected();
 void exportMidi();
 InversoDrumProcessor& processor;
 StepGrid grid;
 std::array<juce::TextButton,8> patternButtons;
 std::array<juce::TextButton,16> padButtons;
 juce::ToggleButton chain{"CHAIN 8"};
 juce::Slider swing,level,pan;
 juce::Label swingLabel,levelLabel,panLabel,selectedSample,tempoLabel;
 juce::TextButton loadButton{"LOAD WAV / AIFF"},exportButton{"EXPORT MIDI"};
 std::unique_ptr<juce::FileChooser> chooser;
 int selectedPad=0;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoDrumEditor)
};
