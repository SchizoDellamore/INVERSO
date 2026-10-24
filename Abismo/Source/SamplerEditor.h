#pragma once
#include <JuceHeader.h>
#include "SoftUI.h"
#include "SamplerProcessor.h"

// Waveform with start / loop / grain-position markers and the live playhead,
// drawn over the SAMPLE module's screen. Dragging moves the nearest marker:
// START / END of the region (and the loop points when looping), or POSITION (granular).
class SampleView final : public juce::Component {
public:
 explicit SampleView(InversoAbismoSamplerProcessor&);
 void refresh();
 juce::String status;
 bool statusIsError=false;
 void paint(juce::Graphics&) override;
 void mouseDown(const juce::MouseEvent&) override;
 void mouseDrag(const juce::MouseEvent&) override;
 void mouseUp(const juce::MouseEvent&) override;
 void resized() override {refresh();}
private:
 void setFromX(float x);
 const char* markerNear(float x) const;
 const char* dragging=nullptr;
 float param(const char* id) const;
 InversoAbismoSamplerProcessor& processor;
 std::vector<std::pair<float,float>> peaks;
};

class InversoAbismoSamplerEditor final : public juce::AudioProcessorEditor,public juce::FileDragAndDropTarget,
                                         private juce::Timer {
public:
 explicit InversoAbismoSamplerEditor(InversoAbismoSamplerProcessor&);
 ~InversoAbismoSamplerEditor() override;
 void paintOverChildren(juce::Graphics&) override;
 void resized() override;
 bool isInterestedInFileDrag(const juce::StringArray&) override;
 void filesDropped(const juce::StringArray&,int,int) override;
 void fileDragEnter(const juce::StringArray&,int,int) override {dragging=true;repaint();}
 void fileDragExit(const juce::StringArray&) override {dragging=false;repaint();}
 // Pull sample, status and meters from the processor (the timer does this 30x/s).
 void refreshFromProcessor();
private:
 void timerCallback() override {refreshFromProcessor();}
 void load(const juce::File&);
 float value(int param) const;
 InversoAbismoSamplerProcessor& processor;
 soft::Look look;
 soft::Body body;
 soft::PresetBar presets;
 std::vector<std::unique_ptr<soft::Module>> modules;
 std::unique_ptr<soft::Master> master;
 SampleView view;
 juce::TextButton loadButton{"LOAD"},builtInButton{"BUILT-IN VOICE"};
 juce::TooltipWindow tooltips{this,600};
 std::unique_ptr<juce::FileChooser> chooser;
 juce::String loadError;
 int shownVersion=-1;
 bool dragging=false;
 float animation=0.f;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoAbismoSamplerEditor)
};
