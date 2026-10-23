#pragma once
#include <JuceHeader.h>
#include "AbismoUI.h"
#include "DrumsProcessor.h"
#include <array>

// One of the 16 pads: note name, object name, a flash when it sounds.
class PadButton final : public juce::Component {
public:
 PadButton(int index,std::function<void(int)> onPress);
 void setObjectName(const juce::String&);
 void setSelected(bool);
 void flash();
 bool decay(); // returns true while still lit
 void paint(juce::Graphics&) override;
 void mouseDown(const juce::MouseEvent&) override;
private:
 int index;
 std::function<void(int)> onPress;
 juce::String objectName;
 bool selected=false;
 float light=0.f;
};

class InversoAbismoDrumsEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
 explicit InversoAbismoDrumsEditor(InversoAbismoDrumsProcessor&);
 ~InversoAbismoDrumsEditor() override;
 void paint(juce::Graphics&) override;
 void resized() override;
 void selectPad(int);
 int selectedPad() const {return selected;}
private:
 void timerCallback() override;
 InversoAbismoDrumsProcessor& processor;
 abismo_ui::Look look;
 std::vector<std::unique_ptr<PadButton>> pads;
 std::vector<std::unique_ptr<abismo_ui::ParameterPanel>> padPanels;
 std::unique_ptr<abismo_ui::ParameterPanel> globals;
 juce::ComboBox kitBox;
 juce::TextButton previous{"<"},next{">"};
 int selected=0;
 float shownPeak=0.f;
 int shownHits=0;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoAbismoDrumsEditor)
};
