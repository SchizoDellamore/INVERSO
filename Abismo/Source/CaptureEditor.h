#pragma once
#include <JuceHeader.h>
#include "SoftUI.h"
#include "CaptureProcessor.h"

// A clip's waveform. The source one carries the region (drag the two ends) and, while the original is heard,
// the playhead; the result one shows what the edit makes of it and the playhead while that plays.
class CaptureWave final : public juce::Component {
public:
 CaptureWave(InversoCaptureProcessor&, bool resultView);
 void refresh();
 void paint(juce::Graphics&) override;
 void mouseDown(const juce::MouseEvent&) override;
 void mouseDrag(const juce::MouseEvent&) override;
 void mouseUp(const juce::MouseEvent&) override;
 void resized() override { peaksFor=~0u; refresh(); }
private:
 void setParam(int p,float percent);
 float percentOf(int p) const;
 InversoCaptureProcessor& processor;
 const bool result;
 std::vector<std::pair<float,float>> peaks;
 unsigned peaksFor=~0u;
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

class InversoCaptureEditor final : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer {
public:
 explicit InversoCaptureEditor(InversoCaptureProcessor&);
 ~InversoCaptureEditor() override;
 void resized() override;
 bool isInterestedInFileDrag(const juce::StringArray&) override;
 void filesDropped(const juce::StringArray&,int,int) override;
 void refresh() { timerCallback(); } // what the timer does, on demand (tests, snapshots)
private:
 void timerCallback() override;
 void say(const juce::String&, bool problem=false);
 InversoCaptureProcessor& processor;
 soft::Look look;
 soft::Body body;
 CaptureWave source, result;
 CaptureDragZone drag;
 juce::TextButton openButton{"OPEN"}, captureButton{"CAPTURE"}, recButton{"REC"}, playButton{"PLAY"}, originalButton{"HEAR ORIGINAL"}, undoButton{"UNDO"}, bakeButton{"BAKE"}, normalizeButton{"NORMALIZE"}, saveButton{"SAVE WAV"};
 juce::Label status, sourceTitle, resultTitle;
 std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> originalAttachment;
 std::vector<std::unique_ptr<soft::Module>> modules;
 std::unique_ptr<juce::FileChooser> chooser;
 int designW=1180, designH=800;
 juce::TooltipWindow tooltips{this, 600};
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoCaptureEditor)
};
