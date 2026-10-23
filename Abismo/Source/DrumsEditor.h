#pragma once
#include <JuceHeader.h>
#include "SoftUI.h"
#include "DrumsProcessor.h"
#include <array>

// The 16 pads as raised keys on a card: note, object name, a glow when they sound.
class PadGrid final : public juce::Component {
public:
 std::function<void(int)> onPress;
 void setPad(int pad,const juce::String& name,bool userSample,bool missing);
 void setSelected(int pad);
 void flash(int pad);
 void decay();
 void setDropping(int pad);
 int padAt(juce::Point<int>) const;
 void paint(juce::Graphics&) override;
 void mouseDown(const juce::MouseEvent&) override;
private:
 juce::Rectangle<float> keyBounds(int pad) const;
 struct Key { juce::String name; bool user=false,missing=false; float light=0.f; };
 std::array<Key,inverso_abismo::drums::numPads> keys;
 int selected=0,dropping=-1;
 juce::Image cardCache;
};

class InversoAbismoDrumsEditor final : public juce::AudioProcessorEditor,public juce::FileDragAndDropTarget,private juce::Timer {
public:
 explicit InversoAbismoDrumsEditor(InversoAbismoDrumsProcessor&);
 ~InversoAbismoDrumsEditor() override;
 void resized() override;
 void selectPad(int);
 int selectedPad() const {return selected;}
 bool isInterestedInFileDrag(const juce::StringArray&) override;
 void fileDragMove(const juce::StringArray&,int x,int y) override;
 void fileDragExit(const juce::StringArray&) override;
 void filesDropped(const juce::StringArray&,int x,int y) override;
private:
 int padAt(int x,int y) const;
 void loadFiles(int pad,const juce::StringArray&);
 void chooseFiles();
 void timerCallback() override;
 float value(int param) const;
 void drawPadScreen(juce::Graphics&,juce::Rectangle<float>,int pad);
 InversoAbismoDrumsProcessor& processor;
 soft::Look look;
 soft::Body body;
 soft::PresetBar kits;
 PadGrid grid;
 std::vector<std::unique_ptr<soft::Module>> padModules,modules;
 std::unique_ptr<soft::Master> master;
 juce::TextButton loadButton{"LOAD SAMPLE"},clearButton{"BACK TO OBJECT"};
 juce::TooltipWindow tooltips{this,600};
 std::unique_ptr<juce::FileChooser> chooser;
 juce::String loadError;
 std::array<float,inverso_abismo::drums::numPads> padLight{};
 int selected=0;
 float animation=0.f;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoAbismoDrumsEditor)
};
