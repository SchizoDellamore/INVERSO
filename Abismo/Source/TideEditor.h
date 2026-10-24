#pragma once
#include <JuceHeader.h>
#include "SoftUI.h"
#include "TideProcessor.h"

// The big screen: the spectrum, the EQ curve as set, the curve as it moves
// right now, and one node per band to drag (left/right: frequency, up/down:
// gain, with ALT or the right button: dynamic range; wheel: width).
class TideGraph final : public juce::Component {
public:
 explicit TideGraph(InversoTideProcessor& p);
 void paint(juce::Graphics&) override;
 void resized() override { cardCache = {}; }
 void mouseMove(const juce::MouseEvent&) override;
 void mouseDown(const juce::MouseEvent&) override;
 void mouseDrag(const juce::MouseEvent&) override;
 void mouseUp(const juce::MouseEvent&) override;
 void mouseDoubleClick(const juce::MouseEvent&) override;
 void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
 void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
 void update();
 std::function<void(int)> onSelect; // a band was clicked
private:
 juce::Rectangle<float> plotArea() const;
 float xOf(double f) const;
 double fOf(float x) const;
 float yOf(double db) const;
 double dbOf(float y) const;
 juce::Point<float> nodeOf(int band) const;
 int hit(juce::Point<float>) const;
 float value(int param) const;
 void set(int param, float value);
 void gesture(int band, bool begin);
 InversoTideProcessor& processor;
 juce::dsp::FFT fft{12};
 std::vector<float> window, work;
 std::vector<float> spectrum; // smoothed dB per display column
 std::array<float, inverso_abismo::tide::numBands> dynamic{};
 int hover = -1, dragging = -1;
 bool dragRange = false;
 juce::Image cardCache;
};

class InversoTideEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
 explicit InversoTideEditor(InversoTideProcessor&);
 ~InversoTideEditor() override;
 void resized() override;
 void refresh() { timerCallback(); } // what the timer does, on demand (tests, snapshots)
private:
 void timerCallback() override;
 InversoTideProcessor& processor;
 soft::Look look;
 soft::Body body;
 soft::PresetBar presets;
 TideGraph graph;
 std::vector<std::unique_ptr<soft::Module>> modules;
 std::array<juce::TextButton, inverso_abismo::tide::numBands> onButtons;
 std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, inverso_abismo::tide::numBands> onAttach;
 std::unique_ptr<soft::Master> master;
 juce::TooltipWindow tooltips{this, 600};
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoTideEditor)
};
