#pragma once
#include <JuceHeader.h>
#include "SoftUI.h"
#include "CaptureProcessor.h"

// The clip's waveform with the region (drag the two ends), the playhead and the live level while REC is on.
class CaptureWave final : public juce::Component {
public:
 explicit CaptureWave(InversoCaptureProcessor&);
 void refresh();
 void paint(juce::Graphics&) override;
 void mouseDown(const juce::MouseEvent&) override;
 void mouseDrag(const juce::MouseEvent&) override;
 void mouseUp(const juce::MouseEvent&) override;
 void resized() override { peaksFor=0; refresh(); }
private:
 void setParam(int p,float percent);
 float percentOf(int p) const;
 InversoCaptureProcessor& processor;
 std::vector<std::pair<float,float>> peaks;
 unsigned peaksFor=0, shownVersion=~0u;
 int dragging=-1; // P::Start or P::End
};

// Drag from here onto a track of the DAW: the edited clip as a WAV file.
class CaptureDragZone final : public juce::Component, public juce::SettableTooltipClient {
public:
 explicit CaptureDragZone(InversoCaptureProcessor& p) : processor(p) { setTooltip("Drag onto a track in your DAW: the edited clip, as a WAV file"); setMouseCursor(juce::MouseCursor::DraggingHandCursor); }
 void paint(juce::Graphics&) override;
 void mouseDrag(const juce::MouseEvent&) override;
 void mouseUp(const juce::MouseEvent&) override;
 juce::String message;
private:
 InversoCaptureProcessor& processor;
 bool dragged=false;
};

class InversoCaptureEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
 explicit InversoCaptureEditor(InversoCaptureProcessor&);
 ~InversoCaptureEditor() override;
 void resized() override;
 void refresh() { timerCallback(); } // what the timer does, on demand (tests, snapshots)
private:
 void timerCallback() override;
 InversoCaptureProcessor& processor;
 soft::Look look;
 soft::Body body;
 CaptureWave wave;
 CaptureDragZone drag;
 juce::TextButton captureButton{"CAPTURE"}, recButton{"REC"}, playButton{"PLAY"}, undoButton{"UNDO"}, bakeButton{"BAKE"}, normalizeButton{"NORMALIZE"}, saveButton{"SAVE WAV"};
 juce::Label status;
 std::unique_ptr<soft::Module> edit, mix;
 std::unique_ptr<juce::FileChooser> chooser;
 int designW=1000, designH=600;
 juce::TooltipWindow tooltips{this, 600};
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoCaptureEditor)
};
